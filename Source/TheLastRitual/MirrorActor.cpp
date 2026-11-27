#include "MirrorActor.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"
#include "ClueComponent.h"
#include "GhostHunterCharacter.h"
#include "NightSubsystem.h"
#include "HorrorSynthWave.h"

#define LOCTEXT_NAMESPACE "GhostHunt"

AMirrorActor::AMirrorActor()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));

	// Placeholder glass: 4 x 60 x 90 cm, facing +X. Hang it on a wall.
	Glass = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Glass"));
	Glass->SetupAttachment(RootComponent);

	// Cloth: slightly bigger and in front of the glass, hidden until covered.
	Cloth = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cloth"));
	Cloth->SetupAttachment(RootComponent);
	Cloth->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (Cube.Succeeded())
	{
		Glass->SetStaticMesh(Cube.Object);
		Glass->SetRelativeScale3D(FVector(0.04f, 0.6f, 0.9f));

		Cloth->SetStaticMesh(Cube.Object);
		Cloth->SetRelativeLocation(FVector(3.f, 0.f, -5.f));
		Cloth->SetRelativeScale3D(FVector(0.02f, 0.7f, 1.05f));
	}

	Clue = CreateDefaultSubobject<UClueComponent>(TEXT("Clue"));
}

void AMirrorActor::BeginPlay()
{
	Super::BeginPlay();

	// The engine Cube uses the world-grid material; give the glass a dark, silvery tint
	// and the cloth an old off-white instead.
	if (UMaterialInterface* Basic = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		auto Tint = [Basic](UStaticMeshComponent* Mesh, const FLinearColor& Color)
		{
			Mesh->SetMaterial(0, Basic);
			if (UMaterialInstanceDynamic* Mat = Mesh->CreateDynamicMaterialInstance(0))
			{
				Mat->SetVectorParameterValue(TEXT("Color"), Color);
			}
		};
		Tint(Glass, FLinearColor(0.06f, 0.07f, 0.075f)); // old, tarnished glass
		Tint(Cloth, FLinearColor(0.55f, 0.5f, 0.42f));   // yellowed muslin
	}

	SetCovered(bStartCovered);
}

void AMirrorActor::SetCovered(bool bInCovered)
{
	bCovered = bInCovered;
	Cloth->SetVisibility(bCovered);
}

bool AMirrorActor::HasUnfoundClue() const
{
	return Clue && !Clue->SignId.IsNone() && !Clue->IsFound();
}

bool AMirrorActor::IsLastUncoveredMirror() const
{
	if (bCovered)
	{
		return false;
	}

	int32 MirrorCount = 0;
	for (TActorIterator<AMirrorActor> It(GetWorld()); It; ++It)
	{
		++MirrorCount;
		if (*It != this && !It->IsCovered())
		{
			return false;
		}
	}
	return MirrorCount > 1; // the trap needs at least one other mirror covered
}

void AMirrorActor::Interact_Implementation(AGhostHunterCharacter* Hunter)
{
	if (HasUnfoundClue())
	{
		if (Clue->MarkFound())
		{
			UHorrorSynthWave::Play(this, EHorrorSfx::ClueFound);
		}
		return;
	}

	UNightSubsystem* Night = GetWorld()->GetSubsystem<UNightSubsystem>();
	if (!bTrapUsed && Night && Night->IsPlaying() && IsLastUncoveredMirror())
	{
		bTrapUsed = true;
		UHorrorSynthWave::Play(this, EHorrorSfx::MirrorCover);
		UHorrorSynthWave::Play(this, Night->PerformRitual(TrapRitualId) ? EHorrorSfx::RitualCorrect : EHorrorSfx::RitualWrong);
		if (Night->GetState() == ENightState::Won)
		{
			SetCovered(true); // she's sealed inside
		}
		return;
	}

	SetCovered(!bCovered);
	UHorrorSynthWave::Play(this, EHorrorSfx::MirrorCover, bCovered ? 1.f : 0.8f);
}

FText AMirrorActor::GetInteractPrompt_Implementation() const
{
	if (HasUnfoundClue())
	{
		return FText::Format(LOCTEXT("MirrorExamine", "Examine {0}"), Clue->DisplayName);
	}

	const UNightSubsystem* Night = GetWorld()->GetSubsystem<UNightSubsystem>();
	if (!bTrapUsed && Night && Night->IsPlaying() && IsLastUncoveredMirror())
	{
		return LOCTEXT("MirrorTrap", "Trap the spirit in this mirror");
	}

	return bCovered ? LOCTEXT("MirrorUncover", "Uncover mirror") : LOCTEXT("MirrorCover", "Cover mirror");
}

#undef LOCTEXT_NAMESPACE
