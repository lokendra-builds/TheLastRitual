#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gadgets.h"
#include "GadgetComponent.generated.h"

class AGhostHunterCharacter;

/**
 * What Arjun has in his hand. Keys 1-5 pick a gadget (if bought), left mouse uses it.
 *
 *   Torch        normal light (F toggles)
 *   Gun Torch    torch + fires a blessed flash: hits the ghost if she's in the beam
 *   UV Torch     purple light; reveals UV-only clues it shines on
 *   Thermometer  reads colder near the ghost and her home
 *   EMF Meter    level 1-5 near the ghost, beeps faster
 */
UCLASS(ClassGroup = (GhostHunt), meta = (BlueprintSpawnableComponent))
class THELASTRITUAL_API UGadgetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGadgetComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Equip by slot (0-4). Does nothing if the gadget isn't owned. */
	UFUNCTION(BlueprintCallable, Category = "Gadgets")
	bool Equip(EGadget Gadget);

	/** Left mouse. */
	UFUNCTION(BlueprintCallable, Category = "Gadgets")
	void Use();

	UFUNCTION(BlueprintPure, Category = "Gadgets")
	EGadget GetEquipped() const { return Equipped; }

	UFUNCTION(BlueprintPure, Category = "Gadgets")
	bool IsOwned(EGadget Gadget) const;

	UFUNCTION(BlueprintPure, Category = "Gadgets")
	int32 GetShotsLeft() const { return ShotsLeft; }

	UFUNCTION(BlueprintPure, Category = "Gadgets")
	float GetTemperature() const { return Temperature; }

	UFUNCTION(BlueprintPure, Category = "Gadgets")
	int32 GetEMFLevel() const { return EMFLevel; }

	/** 0..1, for the muzzle flash on the HUD. */
	float GetFlashAlpha() const { return FlashAlpha; }

	/** True for a moment after a shot hit her (hit marker). */
	bool WasRecentHit() const { return HitMarkerLeft > 0.f; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gadgets|Gun Torch")
	int32 ShotsPerNight = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gadgets|Gun Torch")
	float ShotDamage = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gadgets|Gun Torch")
	float ShotRange = 2200.f;

	/** Half-angle of the beam cone that counts as a hit (degrees). Forgiving on purpose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gadgets|Gun Torch")
	float ShotConeDegrees = 7.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gadgets|Gun Torch")
	float ShotCooldown = 0.45f;

protected:
	virtual void BeginPlay() override;

private:
	AGhostHunterCharacter* GetHunter() const;
	void ApplyLightForGadget();
	void FireGunTorch();
	void SpawnImpact(const struct FHitResult& Hit, const FVector& Aim);
	void TickUV();
	void TickSensors(float DeltaTime);

	EGadget Equipped = EGadget::Torch;
	int32 ShotsLeft = 12;
	float CooldownLeft = 0.f;
	float FlashAlpha = 0.f;
	float HitMarkerLeft = 0.f;
	float Temperature = 24.f;
	int32 EMFLevel = 1;
	float SensorTimer = 0.f;
};
