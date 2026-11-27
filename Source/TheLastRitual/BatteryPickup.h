#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "BatteryPickup.generated.h"

class UStaticMeshComponent;

/** A torch battery lying around the house. Press E to refill the torch. */
UCLASS()
class THELASTRITUAL_API ABatteryPickup : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ABatteryPickup();

	virtual void Interact_Implementation(AGhostHunterCharacter* Hunter) override;
	virtual FText GetInteractPrompt_Implementation() const override;

	/** Battery added, same units as the torch's MaxBattery (100 = full). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battery")
	float Amount = 50.f;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;
};
