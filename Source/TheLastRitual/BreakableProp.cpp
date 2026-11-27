#include "BreakableProp.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "GhostHunterCharacter.h"
#include "HorrorSynthWave.h"
#include "Kismet/GameplayStatics.h"

ABreakableProp::ABreakableProp()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));

	// Default look: a clay pot (sphere), so a bare actor still reads as something.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Mesh->SetStaticMesh(Sphere.Object);
		Mesh->SetRelativeScale3D(FVector(0.4f, 0.4f, 0.5f));
	}

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Mesh);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(4.f);
	Light->SetAttenuationRadius(500.f);
	Light->SetLightColor(FLinearColor(1.f, 0.55f, 0.18f));
	Light->SetVisibility(false);
}

void ABreakableProp::BeginPlay()
{
	Super::BeginPlay();
	Light->SetVisibility(bHasLight);
}

void ABreakableProp::Break(const FVector& HitPoint, const FVector& ShotDirection)
{
	if (bBroken)
	{
		return;
	}
	bBroken = true;

	UWorld* World = GetWorld();
	UStaticMesh* ShardMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* ShardMaterial = Mesh->GetMaterial(0);
	const FBox Bounds = Mesh->Bounds.GetBox();
	const FVector Extent = Bounds.GetExtent();
	const float Size = FMath::Clamp(Extent.GetMax() * 0.35f, 4.f, 25.f); // shard edge length (cm)

	// Splinters are long and thin; pottery and glass are flat curved-ish chips.
	FVector ShardShape(1.f, 0.7f, 0.25f);
	if (Kind == EBreakableKind::Crate)
	{
		ShardShape = FVector(1.6f, 0.25f, 0.12f);
	}
	else if (Kind == EBreakableKind::Glass || Kind == EBreakableKind::Lantern)
	{
		ShardShape = FVector(0.8f, 0.6f, 0.06f);
	}

	for (int32 i = 0; i < ShardCount && ShardMesh; ++i)
	{
		const FVector Spawn = Bounds.GetCenter() + FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-0.6f, 0.8f)) * Extent * 0.7f;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Shard = World->SpawnActor<AStaticMeshActor>(Spawn, FRotator(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f)), Params);
		if (!Shard)
		{
			continue;
		}
		UStaticMeshComponent* C = Shard->GetStaticMeshComponent();
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(ShardMesh);
		if (ShardMaterial)
		{
			C->SetMaterial(0, ShardMaterial);
		}
		const float S = Size / 100.f * FMath::FRandRange(0.5f, 1.2f);
		C->SetWorldScale3D(ShardShape * S);
		C->SetCollisionProfileName(TEXT("PhysicsActor"));
		C->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore); // never trip the player
		C->SetCanEverAffectNavigation(false);
		C->SetSimulatePhysics(true);
		C->SetMassOverrideInKg(NAME_None, 0.2f, true);
		// Away from the shot, plus a pop upwards.
		const FVector Out = (Spawn - HitPoint).GetSafeNormal() * FMath::FRandRange(150.f, 380.f) + ShotDirection.GetSafeNormal() * 220.f + FVector(0.f, 0.f, FMath::FRandRange(120.f, 320.f));
		C->SetPhysicsLinearVelocity(Out);
		C->SetPhysicsAngularVelocityInDegrees(FVector(FMath::FRandRange(-720.f, 720.f), FMath::FRandRange(-720.f, 720.f), FMath::FRandRange(-720.f, 720.f)));
		Shard->SetLifeSpan(FMath::FRandRange(10.f, 16.f));
	}

	if (Light)
	{
		Light->SetVisibility(false);
	}

	if (AGhostHunterCharacter* Hunter = Cast<AGhostHunterCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		if (UHorrorSynthWave* Audio = Hunter->GetHorrorAudio())
		{
			Audio->PlaySfx(Kind == EBreakableKind::Crate ? EHorrorSfx::DoorSlam : EHorrorSfx::DoorLocked, 0.9f);
		}
	}
	Destroy();
}
