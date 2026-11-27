#include "ClueActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "ClueComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HorrorSynthWave.h"

#define LOCTEXT_NAMESPACE "GhostHunt"

AClueActor::AClueActor()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;

	Clue = CreateDefaultSubobject<UClueComponent>(TEXT("Clue"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Mesh->SetStaticMesh(Cube.Object);
		Mesh->SetRelativeScale3D(FVector(0.2f, 0.2f, 0.05f));
	}
}

void AClueActor::BeginPlay()
{
	Super::BeginPlay();
	if (bRequiresUV)
	{
		SetActorHiddenInGame(true);
		SetActorEnableCollision(false);
	}
}

void AClueActor::RevealUV()
{
	bUVRevealed = true;
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	// Glowing violet marks. The engine Cube uses the world-grid material (no "Color"
	// parameter), so switch to the basic shape material first.
	if (UMaterialInterface* Basic = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		Mesh->SetMaterial(0, Basic);
	}
	if (Mesh && Mesh->GetMaterial(0))
	{
		if (UMaterialInstanceDynamic* Mat = Mesh->CreateDynamicMaterialInstance(0))
		{
			Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.7f, 0.3f, 1.f));
		}
	}
}

void AClueActor::Interact_Implementation(AGhostHunterCharacter* Hunter)
{
	if (!Clue || Clue->IsFound())
	{
		return;
	}

	if (!Clue->MarkFound())
	{
		return;
	}
	UHorrorSynthWave::Play(this, EHorrorSfx::ClueFound);
	if (bPickUp)
	{
		UHorrorSynthWave::Play(this, EHorrorSfx::Pickup);
		Destroy();
	}
}

FText AClueActor::GetInteractPrompt_Implementation() const
{
	if (!Clue)
	{
		return FText::GetEmpty();
	}
	if (Clue->IsFound())
	{
		return FText::Format(LOCTEXT("ClueExamined", "{0} (in Ghost Guide)"), Clue->DisplayName);
	}
	return bPickUp
		? FText::Format(LOCTEXT("CluePickUp", "Pick up {0}"), Clue->DisplayName)
		: FText::Format(LOCTEXT("ClueExamine", "Examine {0}"), Clue->DisplayName);
}

#undef LOCTEXT_NAMESPACE
