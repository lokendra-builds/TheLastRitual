#include "RitualSpot.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
#include "GhostHunterCharacter.h"
#include "NightSubsystem.h"
#include "HorrorSynthWave.h"

#define LOCTEXT_NAMESPACE "GhostHunt"

ARitualSpot::ARitualSpot()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;

	// Placeholder: a low wide disc, like a ritual mark on the floor.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		Mesh->SetStaticMesh(Cylinder.Object);
		Mesh->SetRelativeScale3D(FVector(1.2f, 1.2f, 0.02f));
	}
}

void ARitualSpot::Interact_Implementation(AGhostHunterCharacter* Hunter)
{
	UNightSubsystem* Night = GetWorld()->GetSubsystem<UNightSubsystem>();
	if (bUsed || !Hunter || !Night || !Night->IsPlaying())
	{
		return;
	}

	for (const FName& Item : RequiredItems)
	{
		if (!Hunter->HasItem(Item))
		{
			Hunter->ShowMessage(FText::Format(LOCTEXT("RitualNeeds", "You need {0}."), RequiredItemsText));
			return;
		}
	}

	for (const FName& Item : RequiredItems)
	{
		Hunter->RemoveItem(Item);
	}

	bUsed = true;
	const bool bCorrect = Night->PerformRitual(RitualId);
	UHorrorSynthWave::Play(this, bCorrect ? EHorrorSfx::RitualCorrect : EHorrorSfx::RitualWrong);
	OnRitualDone(bCorrect);
}

FText ARitualSpot::GetInteractPrompt_Implementation() const
{
	return bUsed ? FText::GetEmpty() : ActionText;
}

#undef LOCTEXT_NAMESPACE
