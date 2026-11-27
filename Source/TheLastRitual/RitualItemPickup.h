#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "RitualItemPickup.generated.h"

class UStaticMeshComponent;

/** Something a ritual needs: salt, an iron nail, her letter, matches. */
UCLASS()
class THELASTRITUAL_API ARitualItemPickup : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ARitualItemPickup();

	virtual void Interact_Implementation(AGhostHunterCharacter* Hunter) override;
	virtual FText GetInteractPrompt_Implementation() const override;

	/** Must match an entry in a RitualSpot's RequiredItems, e.g. "Salt". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FText DisplayName;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;
};
