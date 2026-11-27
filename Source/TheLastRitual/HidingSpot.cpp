#include "HidingSpot.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "GhostHunterCharacter.h"

#define LOCTEXT_NAMESPACE "GhostHunt"

AHidingSpot::AHidingSpot()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Floor"));

	// Placeholder almirah: 70 deep x 110 wide x 210 tall, standing on the actor origin.
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Body->SetStaticMesh(Cube.Object);
		Body->SetRelativeScale3D(FVector(0.7f, 1.1f, 2.1f));
		Body->SetRelativeLocation(FVector(0.f, 0.f, 105.f));
	}

	DisplayName = LOCTEXT("Almirah", "almirah");
}

FVector AHidingSpot::GetHideLocation() const
{
	// Capsule centre: just above the floor, slightly back from the doors.
	return GetActorTransform().TransformPosition(FVector(-5.f, 0.f, 92.f));
}

FVector AHidingSpot::GetExitLocation() const
{
	return GetActorTransform().TransformPosition(FVector(95.f, 0.f, 92.f));
}

void AHidingSpot::Interact_Implementation(AGhostHunterCharacter* Hunter)
{
	if (Hunter && !Hunter->IsHiding())
	{
		Hunter->EnterHiding(this);
	}
}

FText AHidingSpot::GetInteractPrompt_Implementation() const
{
	return FText::Format(LOCTEXT("HideIn", "Hide in the {0}"), DisplayName);
}

#undef LOCTEXT_NAMESPACE
