#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "ClueActor.generated.h"

class UStaticMeshComponent;
class UClueComponent;

/**
 * A ready-made clue to drop in a level: a mesh + a ClueComponent.
 * Press E to examine it, which records it in the Ghost Guide.
 * Set bPickUp for objects Arjun takes with him (they vanish once found).
 * Swap the placeholder mesh for a real prop in the editor.
 */
UCLASS()
class THELASTRITUAL_API AClueActor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AClueActor();

	virtual void Interact_Implementation(AGhostHunterCharacter* Hunter) override;
	virtual FText GetInteractPrompt_Implementation() const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clue")
	bool bPickUp = false;

	/** Invisible (and untouchable) until the UV Torch shines on it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clue")
	bool bRequiresUV = false;

	bool NeedsUVReveal() const { return bRequiresUV && !bUVRevealed; }
	void RevealUV();

protected:
	virtual void BeginPlay() override;

private:
	bool bUVRevealed = false;

public:

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UClueComponent> Clue;
};
