#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "RitualSpot.generated.h"

class UStaticMeshComponent;

/**
 * A place where a ritual can be performed: the well (salt circle), the
 * prayer room bell, a doorway (iron nail), the shrine lamp (burn the letter).
 *
 * Any ritual can be performed on any night. If it isn't tonight's ghost's
 * ritual, the player loses a life. That's the "wrong guess" punishment.
 * Each spot can be used once per night.
 */
UCLASS()
class THELASTRITUAL_API ARitualSpot : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ARitualSpot();

	virtual void Interact_Implementation(AGhostHunterCharacter* Hunter) override;
	virtual FText GetInteractPrompt_Implementation() const override;

	/** Must match UGhostDefinition::RitualId of the ghost it banishes, e.g. "SaltCircle". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ritual")
	FName RitualId;

	/** Shown in the prompt, e.g. "Pour a salt circle around the well". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ritual")
	FText ActionText;

	/** Item IDs consumed by the ritual (see RitualItemPickup). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ritual")
	TArray<FName> RequiredItems;

	/** How the missing items are described, e.g. "salt". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ritual")
	FText RequiredItemsText;

	UFUNCTION(BlueprintImplementableEvent, Category = "Ritual")
	void OnRitualDone(bool bCorrect);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

private:
	bool bUsed = false;
};
