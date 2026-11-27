#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "HidingSpot.generated.h"

class UStaticMeshComponent;

/**
 * An almirah (cupboard) the player can hide in. Press E to get in, E to get out.
 *
 * While hiding, the ghost can't see you, unless she watched you get in.
 * She may still come close and wait outside...
 *
 * Opening faces local +X. Place it against a wall with +X pointing into the room.
 */
UCLASS()
class THELASTRITUAL_API AHidingSpot : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AHidingSpot();

	virtual void Interact_Implementation(AGhostHunterCharacter* Hunter) override;
	virtual FText GetInteractPrompt_Implementation() const override;

	/** Where the player's capsule goes while hiding. */
	FVector GetHideLocation() const;

	/** Where the player steps out to. */
	FVector GetExitLocation() const;

	FRotator GetLookOutRotation() const { return GetActorRotation(); }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hiding")
	FText DisplayName;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Body;
};
