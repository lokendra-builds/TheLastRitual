#include "HouseDoor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "GhostHunterCharacter.h"
#include "HorrorSynthWave.h"

#define LOCTEXT_NAMESPACE "GhostHunt"

AHouseDoor::AHouseDoor()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	Hinge->SetupAttachment(Root);

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(Hinge);
	// Doors don't cut the nav mesh: the ghost walks through closed doors.
	// A closed door still blocks her line of sight, which is how you hide.
	DoorMesh->SetCanEverAffectNavigation(false);

	// Placeholder grey-box door: engine cube scaled to 100 x 8 x 210 cm.
	// Swap the mesh in the editor once you have a real door from Fab.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		DoorMesh->SetStaticMesh(Cube.Object);
		DoorMesh->SetRelativeLocation(FVector(0.f, 50.f, 105.f));
		DoorMesh->SetRelativeScale3D(FVector(0.08f, 1.f, 2.1f));
	}
}

void AHouseDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!FMath::IsNearlyEqual(CurrentYaw, TargetYaw, 0.05f))
	{
		CurrentYaw = FMath::FInterpTo(CurrentYaw, TargetYaw, DeltaSeconds, CurrentSpeed);
		Hinge->SetRelativeRotation(FRotator(0.f, CurrentYaw, 0.f));
	}
}

void AHouseDoor::SetOpen(bool bOpen, bool bTowardsPositiveX, bool bSlam)
{
	const bool bWasOpen = bIsOpen;
	bIsOpen = bOpen;
	if (bSlam)
	{
		UHorrorSynthWave::PlayAt(this, EHorrorSfx::DoorSlam, GetActorLocation(), 1.f, /*MinVolume*/ 0.5f);
	}
	else if (bOpen != bWasOpen)
	{
		UHorrorSynthWave::PlayAt(this, bOpen ? EHorrorSfx::DoorOpen : EHorrorSfx::DoorClose, GetActorLocation());
	}
	CurrentSpeed = bSlam ? SlamSpeed : OpenSpeed;

	// Door lies along +Y. Positive yaw swings it towards -X, negative towards +X.
	TargetYaw = bOpen ? (bTowardsPositiveX ? -OpenAngle : OpenAngle) : 0.f;
}

void AHouseDoor::Interact_Implementation(AGhostHunterCharacter* Hunter)
{
	if (bLocked)
	{
		UHorrorSynthWave::Play(this, EHorrorSfx::DoorLocked);
		return;
	}

	if (bIsOpen)
	{
		SetOpen(false);
		return;
	}

	// Swing away from the player.
	bool bPlayerOnPositiveX = true;
	if (Hunter)
	{
		const FVector ToHunter = Hunter->GetActorLocation() - GetActorLocation();
		bPlayerOnPositiveX = FVector::DotProduct(GetActorForwardVector(), ToHunter) >= 0.f;
	}
	SetOpen(true, /*bTowardsPositiveX*/ !bPlayerOnPositiveX);
}

FText AHouseDoor::GetInteractPrompt_Implementation() const
{
	if (bLocked)
	{
		return LOCTEXT("DoorLocked", "Locked");
	}
	return bIsOpen ? LOCTEXT("DoorClose", "Close door") : LOCTEXT("DoorOpen", "Open door");
}

#undef LOCTEXT_NAMESPACE
