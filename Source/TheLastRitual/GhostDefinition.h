#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GhostDefinition.generated.h"

/** One sign a ghost leaves behind, e.g. "CombedHair". Clues in the level point at a SignId. */
USTRUCT(BlueprintType)
struct FGhostSign
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost")
	FName SignId;

	/** How the Ghost Guide describes this sign, e.g. "Combed hair on the floor". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost")
	FText Description;
};

/**
 * One ghost, filled in as a data asset (right-click > Miscellaneous > Data Asset > GhostDefinition).
 * Adding a new ghost = new asset in /Game/Ghosts, no code.
 */
UCLASS(BlueprintType)
class THELASTRITUAL_API UGhostDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost")
	FName GhostId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost")
	FText DisplayName;

	/** Grandfather's note in the Ghost Guide. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost", meta = (MultiLine = true))
	FText Lore;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost")
	TArray<FGhostSign> Signs;

	/** Which ritual banishes it. Ritual spots check this (week 3). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ritual")
	FName RitualId;

	/** What the Ghost Guide says about the weakness. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ritual", meta = (MultiLine = true))
	FText RitualHint;

	UFUNCTION(BlueprintPure, Category = "Ghost")
	bool HasSign(FName SignId) const
	{
		return Signs.ContainsByPredicate([SignId](const FGhostSign& Sign) { return Sign.SignId == SignId; });
	}
};
