#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ClueComponent.generated.h"

UENUM(BlueprintType)
enum class EClueType : uint8
{
	Visual,
	Photo,
	Sound,
	Object,
	Written,
	Gadget
};

/**
 * Add to any actor to make it a clue. SignId must match a sign in some
 * UGhostDefinition. A red herring is simply a clue whose sign belongs to a
 * different ghost than tonight's; bIsRedHerring is a note for level design
 * and debugging only, the player never sees it.
 */
UCLASS(ClassGroup = (GhostHunt), meta = (BlueprintSpawnableComponent))
class THELASTRITUAL_API UClueComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clue")
	FName SignId;

	/** Short name shown in the Ghost Guide, e.g. "Combed hair". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clue")
	FText DisplayName;

	/** Arjun's note about it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clue", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clue")
	EClueType ClueType = EClueType::Visual;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clue|Design")
	bool bIsRedHerring = false;

	/** Records this clue in the Ghost Guide. Returns false if it was already found. */
	UFUNCTION(BlueprintCallable, Category = "Clue")
	bool MarkFound();

	UFUNCTION(BlueprintPure, Category = "Clue")
	bool IsFound() const { return bFound; }

private:
	bool bFound = false;
};
