#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "HouseDoor.generated.h"

class UStaticMeshComponent;

/**
 * A hinged door. Opens away from the player. Can be locked.
 * The hinge is at the actor origin; the door extends along local +Y.
 * Ghost events can call SetOpen(..., bSlam = true).
 */
UCLASS()
class THELASTRITUAL_API AHouseDoor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AHouseDoor();

	virtual void Tick(float DeltaSeconds) override;

	virtual void Interact_Implementation(AGhostHunterCharacter* Hunter) override;
	virtual FText GetInteractPrompt_Implementation() const override;

	/** Open or close. bTowardsPositiveX picks the swing side; bSlam makes it fast. */
	UFUNCTION(BlueprintCallable, Category = "Door")
	void SetOpen(bool bOpen, bool bTowardsPositiveX = false, bool bSlam = false);

	UFUNCTION(BlueprintCallable, Category = "Door")
	void SetLocked(bool bInLocked) { bLocked = bInLocked; }

	UFUNCTION(BlueprintPure, Category = "Door")
	bool IsOpen() const { return bIsOpen; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	bool bLocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	float OpenAngle = 95.f;

	/** Higher = faster. Normal opening. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	float OpenSpeed = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door")
	float SlamSpeed = 18.f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Hinge;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

private:
	bool bIsOpen = false;
	float CurrentYaw = 0.f;
	float TargetYaw = 0.f;
	float CurrentSpeed = 3.f;
};
