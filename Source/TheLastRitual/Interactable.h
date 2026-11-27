#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

class AGhostHunterCharacter;

UINTERFACE(MinimalAPI, Blueprintable)
class UInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Anything the player can press E on: doors, pickups, clues, ritual spots.
 * Implement in C++ (override the _Implementation functions) or in Blueprint.
 */
class THELASTRITUAL_API IInteractable
{
	GENERATED_BODY()

public:
	/** Called when the player presses E while looking at this actor. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(AGhostHunterCharacter* Hunter);

	/** Short text shown on screen, e.g. "Open door". Empty = no prompt. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractPrompt() const;
};
