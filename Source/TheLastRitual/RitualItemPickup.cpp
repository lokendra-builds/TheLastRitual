#include "RitualItemPickup.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "GhostHunterCharacter.h"
#include "HorrorSynthWave.h"

#define LOCTEXT_NAMESPACE "GhostHunt"

ARitualItemPickup::ARitualItemPickup()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		Mesh->SetStaticMesh(Cylinder.Object);
		Mesh->SetRelativeScale3D(FVector(0.1f, 0.1f, 0.15f));
	}
}

void ARitualItemPickup::Interact_Implementation(AGhostHunterCharacter* Hunter)
{
	if (!Hunter)
	{
		return;
	}
	Hunter->AddItem(ItemId);
	UHorrorSynthWave::Play(this, EHorrorSfx::Pickup);
	Hunter->ShowMessage(FText::Format(LOCTEXT("ItemTaken", "Took: {0}"), DisplayName));
	Destroy();
}

FText ARitualItemPickup::GetInteractPrompt_Implementation() const
{
	return FText::Format(LOCTEXT("ItemPickUp", "Take {0}"), DisplayName);
}

#undef LOCTEXT_NAMESPACE
