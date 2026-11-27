#include "GhostGuideSubsystem.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/World.h"
#include "GhostDefinition.h"

void UGhostGuideSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadGhostDefinitions();
}

bool UGhostGuideSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UGhostGuideSubsystem::LoadGhostDefinitions()
{
	Ghosts.Reset();

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	// Outside a cooked build the registry may still be scanning at startup; make sure the ghosts folder is known.
	Registry.ScanPathsSynchronous({ TEXT("/Game/Ghosts") }, /*bForceRescan*/ false);

	TArray<FAssetData> Assets;
	Registry.GetAssetsByClass(UGhostDefinition::StaticClass()->GetClassPathName(), Assets, /*bSearchSubClasses*/ true);

	for (const FAssetData& Asset : Assets)
	{
		if (UGhostDefinition* Ghost = Cast<UGhostDefinition>(Asset.GetAsset()))
		{
			Ghosts.Add(Ghost);
		}
	}

	// Stable order for the guide: by asset name (Ghost_01_..., Ghost_02_...).
	Ghosts.Sort([](const UGhostDefinition& A, const UGhostDefinition& B) { return A.GetName() < B.GetName(); });

	UE_LOG(LogTemp, Log, TEXT("Ghost Guide: loaded %d ghost definitions."), Ghosts.Num());
}

void UGhostGuideSubsystem::RecordClue(const UClueComponent* Clue)
{
	if (!Clue)
	{
		return;
	}

	FFoundClue Found;
	Found.SignId = Clue->SignId;
	Found.DisplayName = Clue->DisplayName;
	Found.Description = Clue->Description;
	Found.ClueType = Clue->ClueType;
	Found.TimeFound = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	FoundClues.Add(Found);
	OnClueRecorded.Broadcast(Found);
}

TArray<UGhostDefinition*> UGhostGuideSubsystem::GetGhosts() const
{
	TArray<UGhostDefinition*> Result;
	for (const TObjectPtr<UGhostDefinition>& Ghost : Ghosts)
	{
		Result.Add(Ghost.Get());
	}
	return Result;
}

bool UGhostGuideSubsystem::HasFoundSign(FName SignId) const
{
	return FoundClues.ContainsByPredicate([SignId](const FFoundClue& Clue) { return Clue.SignId == SignId; });
}

TArray<FGhostMatch> UGhostGuideSubsystem::GetGhostMatches() const
{
	// Unique signs found (two hair clues still count as one sign).
	TSet<FName> FoundSigns;
	for (const FFoundClue& Clue : FoundClues)
	{
		FoundSigns.Add(Clue.SignId);
	}

	TArray<FGhostMatch> Matches;
	for (UGhostDefinition* Ghost : Ghosts)
	{
		if (!Ghost)
		{
			continue;
		}

		FGhostMatch Match;
		Match.Ghost = Ghost;
		Match.SignsTotal = Ghost->Signs.Num();
		for (const FGhostSign& Sign : Ghost->Signs)
		{
			if (FoundSigns.Contains(Sign.SignId))
			{
				++Match.SignsFound;
			}
		}
		Matches.Add(Match);
	}

	Matches.StableSort([](const FGhostMatch& A, const FGhostMatch& B) { return A.SignsFound > B.SignsFound; });
	return Matches;
}

void UGhostGuideSubsystem::ResetNight()
{
	FoundClues.Reset();
}
