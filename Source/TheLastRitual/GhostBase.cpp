#include "GhostBase.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "GhostHunterCharacter.h"
#include "HorrorSynthWave.h"
#include "NightSubsystem.h"
#include "ProgressSubsystem.h"
#include "TorchComponent.h"

AGhostBase::AGhostBase()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Feet"));

	Figure = CreateDefaultSubobject<USceneComponent>(TEXT("Figure"));
	Figure->SetupAttachment(RootComponent);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GhostMat(TEXT("/Game/Materials/M_Ghost.M_Ghost"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	GhostMaterial = GhostMat.Succeeded() ? GhostMat.Object : (ShapeMat.Succeeded() ? ShapeMat.Object : nullptr);

	UStaticMesh* Cyl = CylinderMesh.Succeeded() ? CylinderMesh.Object : nullptr;
	UStaticMesh* Sph = SphereMesh.Succeeded() ? SphereMesh.Object : nullptr;
	UStaticMesh* Box = CubeMesh.Succeeded() ? CubeMesh.Object : nullptr;

	// The Mirror Woman, ~172 cm, facing +X, actor origin at her feet. Sizes in cm.
	const FLinearColor Sari(0.27f, 0.26f, 0.235f);
	const FLinearColor SariDirty(0.17f, 0.155f, 0.13f);
	const FLinearColor Skin(0.24f, 0.25f, 0.245f);
	const FLinearColor Hair(0.006f, 0.005f, 0.005f);
	const FLinearColor Blood(0.10f, 0.004f, 0.003f);
	const float SariGlow = 0.008f, SariRim = 0.12f;

	// Sari: floor-length, many thin tiers so the taper from hem to waist reads as one smooth shape.
	const int32 Tiers = 8;
	for (int32 T = 0; T < Tiers; ++T)
	{
		const float A = T / float(Tiers - 1);
		const float Z0 = T * 14.f;
		const float Depth = FMath::Lerp(44.f, 25.f, FMath::Pow(A, 0.8f));
		const float Width = FMath::Lerp(50.f, 30.f, FMath::Pow(A, 0.8f));
		AddPart(*FString::Printf(TEXT("Sari%d"), T), Cyl, Figure, FVector(0.f, 0.f, Z0 + 8.f), FVector(Depth, Width, 16.f), FRotator::ZeroRotator,
			T == 0 ? SariDirty : Sari, SariGlow, SariRim);
	}
	Body = AddPart(TEXT("Body"), Cyl, Figure, FVector(0.f, 0.f, 122.f), FVector(22.f, 29.f, 26.f), FRotator::ZeroRotator, Sari, SariGlow, SariRim);
	AddPart(TEXT("Chest"), Cyl, Figure, FVector(0.f, 0.f, 136.f), FVector(20.f, 31.f, 10.f), FRotator::ZeroRotator, Sari, SariGlow, SariRim);
	AddPart(TEXT("Shoulders"), Cyl, Figure, FVector(-1.f, 0.f, 141.f), FVector(13.f, 13.f, 34.f), FRotator(0.f, 0.f, 90.f), Sari, SariGlow, SariRim);
	// Pallu: the loose end of the sari hanging down her back.
	AddPart(TEXT("PalluBack"), Box, Figure, FVector(-12.f, 5.f, 100.f), FVector(2.f, 17.f, 84.f), FRotator(-4.f, 0.f, 6.f), SariDirty, SariGlow, SariRim);
	// Old blood soaked into the cloth.
	AddPart(TEXT("Blood0"), Box, Figure, FVector(16.4f, -5.f, 66.f), FVector(1.f, 4.f, 14.f), FRotator(0.f, 0.f, 12.f), Blood, 0.f, 0.f, false);
	AddPart(TEXT("Blood2"), Box, Figure, FVector(22.2f, -8.f, 13.f), FVector(1.f, 9.f, 7.f), FRotator::ZeroRotator, Blood, 0.f, 0.f, false);

	// Neck and head (the face is hidden behind the hair).
	AddPart(TEXT("Neck"), Cyl, Figure, FVector(0.f, 0.f, 152.f), FVector(8.f, 8.f, 14.f), FRotator::ZeroRotator, Skin, 0.04f, 0.3f);
	AddPart(TEXT("Head"), Sph, Figure, FVector(0.f, 0.f, 164.f), FVector(20.f, 17.f, 24.f), FRotator(6.f, 0.f, 0.f), Skin * 0.2f, 0.f, 0.f);

	// Thin pale arms hanging straight down, long fingers.
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		const float Y = Side * 15.5f;
		const TCHAR* S = Side < 0 ? TEXT("L") : TEXT("R");
		AddPart(*FString::Printf(TEXT("UpperArm%s"), S), Cyl, Figure, FVector(1.5f, Y + Side * 1.f, 124.f), FVector(5.5f, 5.5f, 34.f), FRotator(-4.f, 0.f, Side * -3.f), Sari, SariGlow, SariRim);
		AddPart(*FString::Printf(TEXT("Forearm%s"), S), Cyl, Figure, FVector(4.f, Y + Side * 2.f, 92.f), FVector(4.f, 4.f, 34.f), FRotator(-9.f, 0.f, Side * -2.f), Skin, 0.04f, 0.4f);
		AddPart(*FString::Printf(TEXT("Hand%s"), S), Box, Figure, FVector(6.5f, Y + Side * 2.8f, 70.f), FVector(2.5f, 5.f, 11.f), FRotator(-8.f, 0.f, 0.f), Skin, 0.04f, 0.4f);
		for (int32 F = 0; F < 3; ++F)
		{
			AddPart(*FString::Printf(TEXT("Finger%s%d"), S, F), Box, Figure, FVector(7.5f, Y + Side * 2.8f + (F - 1) * 1.5f, 58.f - FMath::Abs(F - 1) * 1.5f),
				FVector(1.1f, 1.1f, 13.f), FRotator(-10.f, 0.f, (F - 1) * 4.f), Skin, 0.04f, 0.4f, false);
		}
	}

	// Hair: a cap over the skull, a long curtain hanging down the back, strands over the shoulders.
	AddPart(TEXT("HairCap"), Sph, Figure, FVector(-1.5f, 0.f, 168.5f), FVector(22.5f, 19.5f, 25.f), FRotator(6.f, 0.f, 0.f), Hair, 0.f, 0.f);
	AddPart(TEXT("HairBackA"), Box, Figure, FVector(-11.f, 0.f, 128.f), FVector(2.f, 17.f, 76.f), FRotator(-6.f, 0.f, 0.f), Hair, 0.f, 0.f);
	AddPart(TEXT("HairBackB"), Box, Figure, FVector(-13.5f, 0.f, 112.f), FVector(1.5f, 13.f, 70.f), FRotator(-3.f, 0.f, 0.f), Hair, 0.f, 0.f);
	for (int32 i = 0; i < 8; ++i)
	{
		const float Y = -10.5f + i * 3.f;
		const float Len = 70.f + (i * 37 % 5) * 7.f;
		AddPart(*FString::Printf(TEXT("HairBack%d"), i), Box, Figure, FVector(-13.f - (i % 2), Y, 176.f - Len * 0.5f), FVector(1.2f, 2.4f, Len),
			FRotator(-4.f, 0.f, (i - 3.5f) * 1.2f), Hair, 0.f, 0.f);
	}
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		for (int32 i = 0; i < 3; ++i)
		{
			const float Len = 58.f + i * 9.f;
			AddPart(*FString::Printf(TEXT("HairSide%s%d"), Side < 0 ? TEXT("L") : TEXT("R"), i), Box, Figure,
				FVector(5.f - i * 3.f, Side * (9.5f + i * 0.8f), 172.f - Len * 0.5f), FVector(1.5f, 2.f, Len), FRotator(0.f, 0.f, Side * -5.f), Hair, 0.f, 0.f);
		}
	}

	// The curtain of hair over her face: pivots at the crown so it can sway. Gaps at the eyes.
	HairFront = CreateDefaultSubobject<USceneComponent>(TEXT("HairFront"));
	HairFront->SetupAttachment(Figure);
	HairFront->SetRelativeLocation(FVector(0.f, 0.f, 176.f));
	// A dark mass of hair behind the strands, so the curtain reads as hair and not as a comb.
	AddPart(TEXT("HairMassFront"), Box, HairFront, FVector(12.f, 0.f, -58.f), FVector(1.f, 15.f, 62.f), FRotator(3.f, 0.f, 0.f), Hair, 0.f, 0.f);
	AddPart(TEXT("HairMassChin"), Box, HairFront, FVector(10.2f, 0.f, -26.f), FVector(1.f, 13.f, 8.f), FRotator(-6.f, 0.f, 0.f), Hair, 0.f, 0.f);
	// Thin, uneven strands; the narrow gaps at +-3.9 cm let the eyes show.
	const float StrandY[] = { -9.6f, -8.5f, -7.4f, -6.3f, -5.2f, -4.6f, -2.9f, -2.0f, -1.0f, 0.1f, 1.1f, 2.1f, 2.9f, 4.7f, 5.4f, 6.5f, 7.6f, 8.7f, 9.6f };
	int32 i = 0;
	for (const float Y : StrandY)
	{
		const float FaceX = 10.8f - Y * Y / 22.f + (i % 3) * 0.35f;
		const float Len = 46.f + ((i * 7) % 5) * 8.f; // below the chin, ragged ends
		const float W = 1.0f + ((i * 5) % 3) * 0.35f;
		// Over the face (crown to chin)...
		AddPart(*FString::Printf(TEXT("HairFace%d"), i), Box, HairFront, FVector(FaceX - 1.6f - FMath::Abs(Y) * 0.05f, Y, -13.f), FVector(0.8f, W, 23.f),
			FRotator(-6.f, 0.f, ((i * 3) % 5 - 2) * 1.5f), Hair, 0.f, 0.f);
		// ...then hanging down over the chest.
		AddPart(*FString::Printf(TEXT("HairChest%d"), i), Box, HairFront, FVector(FaceX + 2.5f, Y * 1.12f, -24.f - Len * 0.5f), FVector(0.8f, W * 1.1f, Len),
			FRotator(3.f, 0.f, Y * 0.5f + ((i * 7) % 5 - 2) * 1.2f), Hair, 0.f, 0.f);
		++i;
	}

	// Two faint red eyes, glinting through the gaps in the hair.
	EyeL = AddPart(TEXT("EyeL"), Sph, Figure, FVector(8.9f, -3.9f, 166.f), FVector(1.4f, 1.6f, 1.1f), FRotator::ZeroRotator, FLinearColor(1.f, 0.03f, 0.01f), 1.2f, 0.f, false);
	EyeR = AddPart(TEXT("EyeR"), Sph, Figure, FVector(8.9f, 3.9f, 166.f), FVector(1.4f, 1.6f, 1.1f), FRotator::ZeroRotator, FLinearColor(1.f, 0.03f, 0.01f), 1.2f, 0.f, false);

	EyeGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("EyeGlow"));
	EyeGlow->SetupAttachment(Figure);
	EyeGlow->SetRelativeLocation(FVector(30.f, 0.f, 160.f)); // a faint red cast in front of her, not on her hair
	EyeGlow->SetIntensityUnits(ELightUnits::Candelas);
	EyeGlow->SetIntensity(0.25f);
	EyeGlow->SetAttenuationRadius(120.f);
	EyeGlow->SetLightColor(FLinearColor(1.f, 0.05f, 0.02f));
	EyeGlow->SetCastShadows(false);
}

UStaticMeshComponent* AGhostBase::AddPart(const TCHAR* Name, UStaticMesh* Mesh, USceneComponent* Parent, const FVector& Loc,
	const FVector& SizeCm, const FRotator& Rot, const FLinearColor& Color, float Glow, float Rim, bool bShadow)
{
	UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Part->SetupAttachment(Parent);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Part->SetGenerateOverlapEvents(false);
	Part->SetCanEverAffectNavigation(false);
	Part->SetCastShadow(bShadow);
	Part->SetReceivesDecals(false);
	if (Mesh)
	{
		Part->SetStaticMesh(Mesh);
	}
	if (GhostMaterial)
	{
		Part->SetMaterial(0, GhostMaterial);
	}
	Part->SetRelativeLocation(Loc);
	Part->SetRelativeRotation(Rot);
	Part->SetRelativeScale3D(SizeCm / 100.f); // engine basic shapes are 100 cm
	Parts.Add(Part);
	PartLooks.Add({ Color, Glow, Rim });
	return Part;
}

void AGhostBase::TickLook(float DeltaSeconds)
{
	// Slow float and sway, only while she can be seen.
	SwayTime += DeltaSeconds;
	const float T = SwayTime;
	if (Figure)
	{
		Figure->SetRelativeLocationAndRotation(
			FVector(0.f, 0.f, 2.f + 1.5f * FMath::Sin(T * 1.3f)),
			FRotator(1.0f * FMath::Sin(T * 0.8f), 0.f, 1.6f * FMath::Sin(T * 0.55f)));
	}
	if (HairFront)
	{
		HairFront->SetRelativeRotation(FRotator(2.f * FMath::Sin(T * 1.7f + 1.f), 0.f, 2.5f * FMath::Sin(T * 1.1f)));
	}
}

void AGhostBase::BeginPlay()
{
	Super::BeginPlay();

	SetActorHiddenInGame(true);
	HomeLocation = GetActorLocation();
	Health = MaxHealth;

	// Per-part colour on the ghost material (Color / Glow / Rim). The engine shape material only has Color.
	for (int32 i = 0; i < Parts.Num() && i < PartLooks.Num(); ++i)
	{
		UStaticMeshComponent* Part = Parts[i];
		if (!Part)
		{
			continue;
		}
		if (GhostMaterial && Part->GetMaterial(0) != GhostMaterial)
		{
			Part->SetMaterial(0, GhostMaterial);
		}
		if (UMaterialInstanceDynamic* Mat = Part->CreateDynamicMaterialInstance(0))
		{
			Mat->SetVectorParameterValue(TEXT("Color"), PartLooks[i].Color);
			Mat->SetScalarParameterValue(TEXT("Glow"), PartLooks[i].Glow);
			Mat->SetScalarParameterValue(TEXT("Rim"), PartLooks[i].Rim);
		}
	}

	if (UNightSubsystem* Night = GetWorld()->GetSubsystem<UNightSubsystem>())
	{
		Night->OnRitualPerformed.AddDynamic(this, &AGhostBase::HandleRitual);
	}
}

void AGhostBase::HandleRitual(FName RitualId, bool bCorrect)
{
	EndHunt();

	if (bCorrect)
	{
		bBanished = true;
		Vanish();
		OnBanished();
		return;
	}

	JumpScare(Cast<AGhostHunterCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)));
}

void AGhostBase::FacePlayer()
{
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
		ToPlayer.Z = 0.f;
		if (!ToPlayer.IsNearlyZero())
		{
			SetActorRotation(ToPlayer.Rotation());
		}
	}
}

void AGhostBase::Appear(FVector FeetLocation, float Duration)
{
	if (bBanished)
	{
		return;
	}

	SetActorLocation(FeetLocation);
	FacePlayer();
	SetActorHiddenInGame(false);
	OnAppeared();

	GetWorldTimerManager().SetTimer(VanishTimer, this, &AGhostBase::Vanish, Duration, false);
}

void AGhostBase::AppearAtRandomPoint(float Duration)
{
	TArray<AActor*> Valid;
	for (AActor* Point : AppearPoints)
	{
		if (Point)
		{
			Valid.Add(Point);
		}
	}
	if (Valid.Num() > 0)
	{
		Appear(Valid[FMath::RandRange(0, Valid.Num() - 1)]->GetActorLocation(), Duration);
	}
}

void AGhostBase::JumpScare(AGhostHunterCharacter* Hunter)
{
	if (bBanished || !Hunter)
	{
		return;
	}

	// 1.2 m in front of the player, on the floor.
	FVector Forward = Hunter->GetActorForwardVector();
	Forward.Z = 0.f;
	Forward.Normalize();

	FVector Feet = Hunter->GetActorLocation() + Forward * 120.f;
	Feet.Z -= Hunter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	Appear(Feet, 0.9f);
	OnJumpScare();
	if (UHorrorSynthWave* Audio = Hunter->GetHorrorAudio())
	{
		Audio->TriggerSting(1.f);
	}

	// When she vanishes, the torch goes dark for a moment.
	TWeakObjectPtr<AGhostHunterCharacter> WeakHunter(Hunter);
	FTimerHandle TorchTimer;
	GetWorldTimerManager().SetTimer(TorchTimer, [WeakHunter]()
	{
		if (WeakHunter.IsValid() && WeakHunter->GetTorch())
		{
			WeakHunter->GetTorch()->KillTorch(1.5f);
		}
	}, 0.9f, false);
}

void AGhostBase::Vanish()
{
	GetWorldTimerManager().ClearTimer(VanishTimer);
	SetActorHiddenInGame(true);
}

// ---------------------------------------------------------------- Combat

void AGhostBase::TakeHit(float Damage)
{
	if (!CanBeHit())
	{
		return;
	}

	Health = FMath::Max(0.f, Health - Damage);
	StaggerLeft = 0.6f;
	UE_LOG(LogTemp, Log, TEXT("Ghost hit for %.0f, health %.0f"), Damage, Health);

	AGhostHunterCharacter* Hunter = Cast<AGhostHunterCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	if (Health > 0.f)
	{
		// Knocked back a little.
		if (Hunter)
		{
			const FVector Away = (GetActorLocation() - Hunter->GetActorLocation()).GetSafeNormal2D();
			SetActorLocation(GetActorLocation() + Away * 60.f);
		}
		return;
	}

	// Driven off: she shrieks and is gone, for now.
	if (Hunter && Hunter->GetHorrorAudio())
	{
		Hunter->GetHorrorAudio()->TriggerSting(1.2f);
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UProgressSubsystem* Progress = GI->GetSubsystem<UProgressSubsystem>())
		{
			Progress->AddPoints(PointsForDrivingOff);
		}
	}
	if (Hunter)
	{
		Hunter->ShowMessage(FText::Format(NSLOCTEXT("GhostHunt", "DrivenOff", "She shrieks and vanishes... for now.  +{0} points"), PointsForDrivingOff), 4.f);
	}
	UE_LOG(LogTemp, Log, TEXT("Ghost driven off"));
	EndHunt();
	OnDrivenOff();
}

// ---------------------------------------------------------------- Hunt

void AGhostBase::StartHunt(float Duration)
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (bBanished || bHunting || !Player)
	{
		return;
	}

	// Spawn at a random appear point that is far enough away (fall back to the furthest one).
	TArray<AActor*> FarPoints;
	AActor* Best = nullptr;
	float BestDist = 0.f;
	for (AActor* Point : AppearPoints)
	{
		if (!Point)
		{
			continue;
		}
		const float Dist = FVector::Dist2D(Point->GetActorLocation(), Player->GetActorLocation());
		if (Dist >= MinSpawnDistance)
		{
			FarPoints.Add(Point);
		}
		if (Dist > BestDist)
		{
			Best = Point;
			BestDist = Dist;
		}
	}
	if (FarPoints.Num() > 0)
	{
		Best = FarPoints[FMath::RandRange(0, FarPoints.Num() - 1)];
		BestDist = FVector::Dist2D(Best->GetActorLocation(), Player->GetActorLocation());
	}
	if (!Best || BestDist < MinSpawnDistance * 0.5f)
	{
		return; // nowhere sensible to come from
	}

	GetWorldTimerManager().ClearTimer(VanishTimer);
	SetActorHiddenInGame(true);
	HuntSpawnLocation = Best->GetActorLocation();

	bHunting = true;
	Health = MaxHealth;
	StaggerLeft = 0.f;
	bHuntWarning = HuntWarningTime > 0.f;
	WarningLeft = HuntWarningTime;
	ChaseTime = 0.f;
	HuntTimeLeft = Duration;
	RepathTimer = 0.f;
	bHasLastSeen = false;
	bSawPlayerHide = false;
	bSeesPrev = false;
	PathPoints.Reset();
	PathIndex = 0;

	UE_LOG(LogTemp, Log, TEXT("Ghost hunt started from %s (%.0f m away), %.0f s warning"), *Best->GetActorNameOrLabel(), BestDist / 100.f, WarningLeft);
	if (!bHuntWarning)
	{
		WarningLeft = 0.f;
	}
}

void AGhostBase::EndHunt(bool bCaughtPlayer)
{
	if (!bHunting)
	{
		return;
	}
	bHunting = false;
	PathPoints.Reset();
	UE_LOG(LogTemp, Log, TEXT("Ghost hunt ended (%s)"), bCaughtPlayer ? TEXT("caught the player") : TEXT("player escaped"));

	if (!bCaughtPlayer)
	{
		Vanish();
	}
	OnHuntEnded(bCaughtPlayer);
}

void AGhostBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsHidden())
	{
		TickLook(DeltaSeconds);
	}
	if (bHunting)
	{
		TickHunt(DeltaSeconds);
	}
}

bool AGhostBase::CanSeePlayer(const AGhostHunterCharacter* Hunter) const
{
	if (!Hunter || !Hunter->GetCamera())
	{
		return false;
	}
	if (Hunter->IsHiding())
	{
		return bSawPlayerHide; // she only knows if she watched you get in
	}
	const FVector Eyes = GetActorLocation() + FVector(0.f, 0.f, 150.f);
	const FVector Target = Hunter->GetCamera()->GetComponentLocation();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GhostSight), false, this);
	Params.AddIgnoredActor(Hunter);
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, Eyes, Target, ECC_Visibility, Params);
}

void AGhostBase::RepathTo(const FVector& Target)
{
	PathPoints.Reset();
	PathIndex = 0;

	if (UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), GetActorLocation(), Target))
	{
		if (Path->IsValid() && Path->PathPoints.Num() > 1)
		{
			PathPoints = Path->PathPoints;
			PathIndex = 1;
			// Partial path (target not reachable on the nav mesh): finish the last bit
			// straight through whatever is in the way. She's a ghost.
			if (Path->IsPartial() && FVector::Dist2D(PathPoints.Last(), Target) > CatchDistance * 0.5f)
			{
				PathPoints.Add(Target);
			}
			return;
		}
	}

	// No nav path (no nav mesh, or target off the mesh): drift straight at it. She's a ghost.
	PathPoints = { GetActorLocation(), Target };
	PathIndex = 1;
}

void AGhostBase::TickHunt(float DeltaSeconds)
{
	AGhostHunterCharacter* Hunter = Cast<AGhostHunterCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	const UNightSubsystem* Night = GetWorld()->GetSubsystem<UNightSubsystem>();
	if (!Hunter || !Night || !Night->IsPlaying())
	{
		EndHunt();
		return;
	}

	HuntTimeLeft -= DeltaSeconds;
	if (HuntTimeLeft <= 0.f)
	{
		EndHunt();
		return;
	}

	const FVector PlayerFeet = Hunter->GetActorLocation() - FVector(0.f, 0.f, Hunter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

	// Warning: she isn't here yet. The torch struggles, the house goes quiet.
	if (bHuntWarning)
	{
		WarningLeft -= DeltaSeconds;
		FlickerTimer -= DeltaSeconds;
		if (FlickerTimer <= 0.f && Hunter->GetTorch())
		{
			Hunter->GetTorch()->KillTorch(FMath::FRandRange(0.05f, 0.25f));
			FlickerTimer = FMath::Lerp(0.9f, 0.2f, GetHuntWarningProgress()) * FMath::FRandRange(0.6f, 1.4f);
		}
		if (WarningLeft <= 0.f)
		{
			bHuntWarning = false;
			SetActorLocation(HuntSpawnLocation);
			FacePlayer();
			SetActorHiddenInGame(false);
			if (UHorrorSynthWave* Audio = Hunter->GetHorrorAudio())
			{
				Audio->TriggerSting(0.8f);
			}
			UE_LOG(LogTemp, Log, TEXT("Ghost manifests: the chase begins"));
			OnHuntStarted();
		}
		return;
	}
	ChaseTime += DeltaSeconds;

	if (StaggerLeft > 0.f)
	{
		StaggerLeft -= DeltaSeconds; // reeling from a hit
		return;
	}

	// Did she watch the player climb into a hiding spot?
	if (Hunter->IsHiding() && Hunter->GetTimeHidden() < 0.6f && bSeesPrev
		&& FVector::Dist2D(GetActorLocation(), PlayerFeet) < 900.f)
	{
		bSawPlayerHide = true;
	}
	const bool bHiddenFromHer = Hunter->IsHiding() && !bSawPlayerHide;

	// Caught?
	if (!bHiddenFromHer && FVector::Dist2D(GetActorLocation(), PlayerFeet) < CatchDistance && FMath::Abs(GetActorLocation().Z - PlayerFeet.Z) < 150.f)
	{
		EndHunt(/*bCaughtPlayer*/ true);
		if (Hunter->IsHiding())
		{
			Hunter->ExitHiding(); // dragged out of the almirah
		}
		if (UNightSubsystem* MutableNight = GetWorld()->GetSubsystem<UNightSubsystem>())
		{
			MutableNight->LoseLife();
		}
		JumpScare(Hunter);
		return;
	}

	// Chase what she can see; otherwise search where she last saw them.
	const bool bSees = CanSeePlayer(Hunter);
	bSeesPrev = bSees;
	if (bSees)
	{
		LastSeenLocation = PlayerFeet;
		bHasLastSeen = true;
	}
	else if (!bHiddenFromHer && bHasLastSeen && FVector::Dist2D(GetActorLocation(), LastSeenLocation) < 60.f)
	{
		EndHunt(); // reached the spot, nobody there
		return;
	}

	FVector Target = (bSees || !bHasLastSeen) ? PlayerFeet : LastSeenLocation;
	if (bHiddenFromHer)
	{
		// She senses you're close. She comes and waits outside, a few steps away...
		const FVector Away = (GetActorLocation() - PlayerFeet).GetSafeNormal2D();
		Target = PlayerFeet + (Away.IsNearlyZero() ? FVector(300.f, 0.f, 0.f) : Away * 300.f);
		if (FVector::Dist2D(GetActorLocation(), Target) < 40.f)
		{
			FacePlayer();
			return;
		}
	}


	RepathTimer -= DeltaSeconds;
	if (RepathTimer <= 0.f || !PathPoints.IsValidIndex(PathIndex))
	{
		RepathTo(Target);
		RepathTimer = 0.4f;
	}

	// Walk the path.
	if (PathPoints.IsValidIndex(PathIndex))
	{
		FVector Next = PathPoints[PathIndex];
		FVector ToNext = Next - GetActorLocation();
		ToNext.Z = 0.f;
		const float Ramp = HuntRampTime > 0.f ? FMath::Clamp(ChaseTime / HuntRampTime, 0.f, 1.f) : 1.f;
		const float Step = HuntSpeed * FMath::Lerp(0.4f, 1.f, Ramp) * DeltaSeconds;
		if (ToNext.Size() <= Step)
		{
			SetActorLocation(FVector(Next.X, Next.Y, Next.Z));
			++PathIndex;
		}
		else
		{
			const FVector Dir = ToNext.GetSafeNormal();
			const float NewZ = FMath::FInterpTo(GetActorLocation().Z, Next.Z, DeltaSeconds, 8.f);
			SetActorLocation(FVector(GetActorLocation().X + Dir.X * Step, GetActorLocation().Y + Dir.Y * Step, NewZ));
			SetActorRotation(Dir.Rotation());
		}
	}

	// While she hunts, the torch can't hold steady.
	FlickerTimer -= DeltaSeconds;
	if (FlickerTimer <= 0.f)
	{
		const float Dist = FVector::Dist2D(GetActorLocation(), PlayerFeet);
		if (Dist < 1500.f && Hunter->GetTorch())
		{
			Hunter->GetTorch()->KillTorch(FMath::FRandRange(0.04f, 0.15f));
		}
		// Closer = faster flicker.
		FlickerTimer = FMath::GetMappedRangeValueClamped(FVector2D(200.f, 1500.f), FVector2D(0.15f, 1.2f), Dist) * FMath::FRandRange(0.6f, 1.4f);
	}
}
