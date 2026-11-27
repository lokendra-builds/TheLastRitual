#include "GadgetComponent.h"

#include "Camera/CameraComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "EngineUtils.h"
#include "ClueActor.h"
#include "BreakableProp.h"
#include "Components/DecalComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GhostBase.h"
#include "GhostHunterCharacter.h"
#include "HorrorSynthWave.h"
#include "ProgressSubsystem.h"
#include "TorchComponent.h"

#define LOCTEXT_NAMESPACE "GhostHunt"

UGadgetComponent::UGadgetComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UGadgetComponent::BeginPlay()
{
	Super::BeginPlay();
	ShotsLeft = ShotsPerNight;
	ApplyLightForGadget();

	// Testing: -AutoEquip=<slot 1-5> (combine with -AllGadgets).
	int32 Slot = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("AutoEquip="), Slot) && Slot >= 1 && Slot <= GadgetTable::Num())
	{
		Equip(static_cast<EGadget>(Slot - 1));
	}
}

AGhostHunterCharacter* UGadgetComponent::GetHunter() const
{
	return Cast<AGhostHunterCharacter>(GetOwner());
}

bool UGadgetComponent::IsOwned(EGadget Gadget) const
{
	const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UProgressSubsystem* Progress = GI ? GI->GetSubsystem<UProgressSubsystem>() : nullptr;
	return Progress ? Progress->Owns(Gadget) : Gadget == EGadget::Torch;
}

bool UGadgetComponent::Equip(EGadget Gadget)
{
	AGhostHunterCharacter* Hunter = GetHunter();
	if (!IsOwned(Gadget))
	{
		if (Hunter)
		{
			Hunter->ShowMessage(FText::Format(LOCTEXT("NotOwned", "You don't have the {0}. Buy it in the shop [B]."),
				FText::FromString(GadgetTable::Info(Gadget).Name)));
		}
		return false;
	}
	Equipped = Gadget;
	ApplyLightForGadget();
	return true;
}

void UGadgetComponent::ApplyLightForGadget()
{
	AGhostHunterCharacter* Hunter = GetHunter();
	UTorchComponent* Torch = Hunter ? Hunter->GetTorch() : nullptr;
	if (!Torch)
	{
		return;
	}

	// The torch beam doubles as the gadget's light. UV is dimmer and purple.
	if (Equipped == EGadget::UVTorch)
	{
		Torch->SetLightColor(FLinearColor(0.45f, 0.15f, 1.f));
		Torch->BaseIntensity = 90.f;
	}
	else
	{
		Torch->SetLightColor(FColor(255, 236, 205));
		Torch->BaseIntensity = Equipped == EGadget::GunTorch ? 240.f : 200.f;
	}
	Torch->SetIntensity(Torch->BaseIntensity);
}

void UGadgetComponent::Use()
{
	if (Equipped == EGadget::GunTorch)
	{
		FireGunTorch();
	}
}

void UGadgetComponent::FireGunTorch()
{
	AGhostHunterCharacter* Hunter = GetHunter();
	if (!Hunter || CooldownLeft > 0.f || Hunter->IsHiding())
	{
		return;
	}
	if (ShotsLeft <= 0)
	{
		Hunter->ShowMessage(LOCTEXT("NoShots", "Out of charges for tonight."), 2.f);
		return;
	}

	--ShotsLeft;
	CooldownLeft = ShotCooldown;
	FlashAlpha = 1.f;
	if (Hunter->GetHorrorAudio())
	{
		Hunter->GetHorrorAudio()->TriggerZap();
	}

	const UCameraComponent* Cam = Hunter->GetCamera();
	if (!Cam)
	{
		return;
	}
	// Aim from the view rotation: it is always current, while the camera component
	// only picks up a new control rotation when the camera updates later in the frame.
	const FVector From = Cam->GetComponentLocation();
	const FVector Aim = Hunter->GetViewRotation().Vector();

	for (TActorIterator<AGhostBase> It(GetWorld()); It; ++It)
	{
		AGhostBase* Ghost = *It;
		if (!Ghost->CanBeHit())
		{
			continue;
		}

		// Aim at her chest; she has no collision, so test the cone, then line of sight.
		const FVector Chest = Ghost->GetActorLocation() + FVector(0.f, 0.f, 110.f);
		const FVector To = Chest - From;
		const float Dist = To.Size();
		const float Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Aim, To.GetSafeNormal())));
		if (Dist > ShotRange || Angle > ShotConeDegrees)
		{
			UE_LOG(LogTemp, Verbose, TEXT("Gun Torch: miss (%.0f m, %.1f deg off)"), Dist / 100.f, Angle);
			continue;
		}

		FCollisionQueryParams Params(SCENE_QUERY_STAT(GunTorch), false, Hunter);
		Params.AddIgnoredActor(Ghost);
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, From, Chest, ECC_Visibility, Params))
		{
			UE_LOG(LogTemp, Verbose, TEXT("Gun Torch: blocked by %s"), *GetNameSafe(Hit.GetActor()));
			continue; // a wall in the way
		}

		Ghost->TakeHit(ShotDamage);
		HitMarkerLeft = 0.25f;
		return; // she took the shot
	}

	// Missed her (or she isn't here): the shot hits whatever is in front of you.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GunTorchImpact), false, Hunter);
	FHitResult Hit;
	// A thin sphere rather than a line, so small things (a 12 cm lantern) are fair to hit.
	if (!GetWorld()->SweepSingleByChannel(Hit, From, From + Aim * ShotRange, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(5.f), Params))
	{
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("Gun Torch shot hit %s (%s)"), *GetNameSafe(Hit.GetActor()), Hit.GetActor() ? *Hit.GetActor()->GetClass()->GetName() : TEXT("-"));

	if (ABreakableProp* Prop = Cast<ABreakableProp>(Hit.GetActor()))
	{
		Prop->Break(Hit.ImpactPoint, Aim);
		return;
	}
	SpawnImpact(Hit, Aim);
}

void UGadgetComponent::SpawnImpact(const FHitResult& Hit, const FVector& Aim)
{
	UWorld* World = GetWorld();

	UE_LOG(LogTemp, Log, TEXT("Gun Torch impact on %s"), *GetNameSafe(Hit.GetActor()));

	// The bullet hole: a dark scorched ring around a black centre, flush with the surface.
	UStaticMesh* Disc = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UMaterialInterface* Basic = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	auto Mark = [&](float Diameter, float Lift, const FLinearColor& Color)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// Cylinder axis is Z: point it along the surface normal.
		const FRotator Rot = FRotationMatrix::MakeFromZ(Hit.ImpactNormal).Rotator();
		AStaticMeshActor* A = World->SpawnActor<AStaticMeshActor>(Hit.ImpactPoint + Hit.ImpactNormal * Lift, Rot, P);
		if (!A || !Disc)
		{
			return;
		}
		UStaticMeshComponent* C = A->GetStaticMeshComponent();
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(Disc);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetCanEverAffectNavigation(false);
		C->SetWorldScale3D(FVector(Diameter / 100.f, Diameter / 100.f, 0.002f));
		if (Basic)
		{
			C->SetMaterial(0, Basic);
			if (UMaterialInstanceDynamic* M = C->CreateDynamicMaterialInstance(0))
			{
				M->SetVectorParameterValue(TEXT("Color"), Color);
			}
		}
		A->SetLifeSpan(120.f);
	};
	const float Spin = FMath::FRandRange(0.8f, 1.2f);
	Mark(7.f * Spin, 0.15f, FLinearColor(0.035f, 0.03f, 0.025f)); // scorch
	Mark(2.6f, 0.3f, FLinearColor(0.f, 0.f, 0.f));                 // hole

	// Chips of whatever was hit fly back out of the surface.
	UStaticMesh* Chip = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* SurfaceMat = nullptr;
	if (const UPrimitiveComponent* Comp = Hit.GetComponent())
	{
		SurfaceMat = Comp->GetMaterial(0);
	}
	for (int32 i = 0; i < 5 && Chip; ++i)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* A = World->SpawnActor<AStaticMeshActor>(Hit.ImpactPoint + Hit.ImpactNormal * 3.f, FRotator(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f), 0.f), P);
		if (!A)
		{
			continue;
		}
		UStaticMeshComponent* C = A->GetStaticMeshComponent();
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(Chip);
		if (SurfaceMat)
		{
			C->SetMaterial(0, SurfaceMat);
		}
		C->SetWorldScale3D(FVector(FMath::FRandRange(0.012f, 0.03f), FMath::FRandRange(0.012f, 0.03f), FMath::FRandRange(0.008f, 0.02f)));
		C->SetCollisionProfileName(TEXT("PhysicsActor"));
		C->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		C->SetCanEverAffectNavigation(false);
		C->SetSimulatePhysics(true);
		const FVector Bounce = FMath::GetReflectionVector(Aim, Hit.ImpactNormal);
		C->SetPhysicsLinearVelocity((Hit.ImpactNormal * 0.7f + Bounce * 0.3f + FMath::VRand() * 0.45f).GetSafeNormal() * FMath::FRandRange(150.f, 400.f));
		A->SetLifeSpan(FMath::FRandRange(4.f, 7.f));
	}

	// A brief spark where it struck.
	FActorSpawnParameters LP;
	LP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (APointLight* Spark = World->SpawnActor<APointLight>(Hit.ImpactPoint + Hit.ImpactNormal * 8.f, FRotator::ZeroRotator, LP))
	{
		Spark->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		if (UPointLightComponent* L = Cast<UPointLightComponent>(Spark->GetLightComponent()))
		{
			L->SetIntensityUnits(ELightUnits::Candelas);
			L->SetIntensity(60.f);
			L->SetAttenuationRadius(220.f);
			L->SetLightColor(FLinearColor(1.f, 0.7f, 0.35f));
			L->SetCastShadows(false);
		}
		Spark->SetLifeSpan(0.08f);
	}

	if (AGhostHunterCharacter* Hunter = GetHunter())
	{
		if (UHorrorSynthWave* Audio = Hunter->GetHorrorAudio())
		{
			Audio->PlaySfx(EHorrorSfx::Footstep, 0.8f); // dull thud of the round hitting stone
		}
	}
}

void UGadgetComponent::TickUV()
{
	AGhostHunterCharacter* Hunter = GetHunter();
	if (!Hunter || !Hunter->GetTorch() || !Hunter->GetTorch()->IsTorchOn() || !Hunter->GetCamera())
	{
		return;
	}

	const FVector From = Hunter->GetCamera()->GetComponentLocation();
	const FVector Aim = Hunter->GetCamera()->GetForwardVector();
	for (TActorIterator<AClueActor> It(GetWorld()); It; ++It)
	{
		AClueActor* Clue = *It;
		if (!Clue->NeedsUVReveal())
		{
			continue;
		}
		const FVector To = Clue->GetActorLocation() - From;
		if (To.Size() > 700.f || FVector::DotProduct(Aim, To.GetSafeNormal()) < FMath::Cos(FMath::DegreesToRadians(22.f)))
		{
			continue;
		}
		FCollisionQueryParams Params(SCENE_QUERY_STAT(UVReveal), false, Hunter);
		Params.AddIgnoredActor(Clue);
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, From, Clue->GetActorLocation(), ECC_Visibility, Params))
		{
			Clue->RevealUV();
			Hunter->ShowMessage(LOCTEXT("UVReveal", "Something glows under the UV light..."), 3.f);
		}
	}
}

void UGadgetComponent::TickSensors(float DeltaTime)
{
	AGhostHunterCharacter* Hunter = GetHunter();
	if (!Hunter)
	{
		return;
	}

	// How close is the spirit (her body, or her home when she's not out)?
	float Near = 99999.f;
	bool bHunting = false;
	for (TActorIterator<AGhostBase> It(GetWorld()); It; ++It)
	{
		if (It->IsBanished())
		{
			continue;
		}
		const float ToHome = FVector::Dist(It->GetHomeLocation(), Hunter->GetActorLocation());
		const float ToBody = It->IsHidden() ? 99999.f : FVector::Dist(It->GetActorLocation(), Hunter->GetActorLocation());
		Near = FMath::Min3(Near, ToHome * 1.3f, ToBody);
		bHunting |= It->IsHunting();
	}

	// Readings update a few times a second, like a real meter.
	SensorTimer -= DeltaTime;
	if (SensorTimer <= 0.f)
	{
		SensorTimer = 0.35f;

		const float Cold = 1.f - FMath::Clamp(Near / 1400.f, 0.f, 1.f);
		Temperature = 24.f - 22.f * Cold + FMath::FRandRange(-0.4f, 0.4f);

		const float Field = 1.f - FMath::Clamp(Near / 1200.f, 0.f, 1.f);
		EMFLevel = FMath::Clamp(1 + FMath::RoundToInt(Field * 4.f + (bHunting ? 1.f : 0.f) + FMath::FRandRange(-0.4f, 0.4f)), 1, 5);
	}

	if (UHorrorSynthWave* Audio = Hunter->GetHorrorAudio())
	{
		const bool bEMF = Equipped == EGadget::EMFMeter;
		Audio->SetBeep(bEMF ? 0.6f + EMFLevel * 1.4f : 0.f, bEMF && EMFLevel >= 2 ? 0.5f : 0.f);
	}
}

void UGadgetComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	CooldownLeft = FMath::Max(0.f, CooldownLeft - DeltaTime);
	FlashAlpha = FMath::Max(0.f, FlashAlpha - DeltaTime * 6.f);
	HitMarkerLeft = FMath::Max(0.f, HitMarkerLeft - DeltaTime);

	if (Equipped == EGadget::UVTorch)
	{
		TickUV();
	}
	TickSensors(DeltaTime);
}

#undef LOCTEXT_NAMESPACE
