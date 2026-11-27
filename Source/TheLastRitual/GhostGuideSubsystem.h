#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ClueComponent.h"
#include "GhostGuideSubsystem.generated.h"

class UGhostDefinition;

USTRUCT(BlueprintType)
struct FFoundClue
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ghost Guide")
	FName SignId;

	UPROPERTY(BlueprintReadOnly, Category = "Ghost Guide")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Ghost Guide")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "Ghost Guide")
	EClueType ClueType = EClueType::Visual;

	/** Seconds since the night started. */
	UPROPERTY(BlueprintReadOnly, Category = "Ghost Guide")
	float TimeFound = 0.f;
};

USTRUCT(BlueprintType)
struct FGhostMatch
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ghost Guide")
	TObjectPtr<UGhostDefinition> Ghost = nullptr;

	/** How many of this ghost's signs the player has found. */
	UPROPERTY(BlueprintReadOnly, Category = "Ghost Guide")
	int32 SignsFound = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ghost Guide")
	int32 SignsTotal = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnClueRecorded, const FFoundClue&, Clue);

/**
 * The Ghost Guide for the current night: which clues were found, and how
 * well each ghost matches them.
 *
 * It never eliminates a ghost by itself. Red herrings mean one found sign
 * can point to the wrong ghost, so the guide only counts matches and the
 * player decides. That's the "think, don't guess" part of the design.
 *
 * Ghosts are every UGhostDefinition asset in the project (keep them in /Game/Ghosts).
 */
UCLASS()
class THELASTRITUAL_API UGhostGuideSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	void RecordClue(const UClueComponent* Clue);

	UFUNCTION(BlueprintPure, Category = "Ghost Guide")
	TArray<FFoundClue> GetFoundClues() const { return FoundClues; }

	UFUNCTION(BlueprintPure, Category = "Ghost Guide")
	bool HasFoundSign(FName SignId) const;

	/** All ghosts with their match counts, best match first. */
	UFUNCTION(BlueprintPure, Category = "Ghost Guide")
	TArray<FGhostMatch> GetGhostMatches() const;

	UFUNCTION(BlueprintPure, Category = "Ghost Guide")
	TArray<UGhostDefinition*> GetGhosts() const;

	/** C++ access without copying. */
	const TArray<FFoundClue>& GetFoundCluesRef() const { return FoundClues; }

	UFUNCTION(BlueprintCallable, Category = "Ghost Guide")
	void ResetNight();

	UPROPERTY(BlueprintAssignable, Category = "Ghost Guide")
	FOnClueRecorded OnClueRecorded;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void LoadGhostDefinitions();

	UPROPERTY()
	TArray<TObjectPtr<UGhostDefinition>> Ghosts;

	TArray<FFoundClue> FoundClues;
};
