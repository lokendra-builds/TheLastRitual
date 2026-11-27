#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "MirrorActor.generated.h"

class UStaticMeshComponent;
class UClueComponent;

/**
 * A mirror in the house. Used by the Mirror Woman's ritual:
 * cover every mirror but one, then trap her in the last one.
 *
 * Press E:
 *   1. first time, if it has a clue (SignId set): examine it
 *   2. if every other mirror is covered: "Trap the spirit in the mirror" (performs ritual MirrorTrap)
 *   3. otherwise: cover / uncover it with a cloth
 */
UCLASS()
class THELASTRITUAL_API AMirrorActor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AMirrorActor();

	virtual void Interact_Implementation(AGhostHunterCharacter* Hunter) override;
	virtual FText GetInteractPrompt_Implementation() const override;

	UFUNCTION(BlueprintCallable, Category = "Mirror")
	void SetCovered(bool bInCovered);

	UFUNCTION(BlueprintPure, Category = "Mirror")
	bool IsCovered() const { return bCovered; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirror")
	FName TrapRitualId = TEXT("MirrorTrap");

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Glass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Cloth;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UClueComponent> Clue;

	UPROPERTY(EditAnywhere, Category = "Mirror")
	bool bStartCovered = false;

private:
	bool IsLastUncoveredMirror() const;
	bool HasUnfoundClue() const;

	bool bCovered = false;
	bool bTrapUsed = false;
};
