#pragma once

#include "CoreMinimal.h"
#include "Components/SpotLightComponent.h"
#include "TorchComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTorchBatteryEmpty);

/**
 * The player's torch: a spotlight with a battery.
 * Drains while on, flickers and dims when low, and can be killed for a few
 * seconds by ghost events (KillTorch).
 */
UCLASS(ClassGroup = (GhostHunt), meta = (BlueprintSpawnableComponent))
class THELASTRITUAL_API UTorchComponent : public USpotLightComponent
{
	GENERATED_BODY()

public:
	UTorchComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Torch")
	void ToggleTorch();

	UFUNCTION(BlueprintCallable, Category = "Torch")
	void SetTorchOn(bool bOn);

	/** Adds battery (same units as MaxBattery). */
	UFUNCTION(BlueprintCallable, Category = "Torch")
	void AddBattery(float Amount);

	/** Ghost event: torch goes dark for this many seconds, even if switched on. */
	UFUNCTION(BlueprintCallable, Category = "Torch")
	void KillTorch(float Seconds);

	UFUNCTION(BlueprintPure, Category = "Torch")
	bool IsTorchOn() const { return bIsOn; }

	UFUNCTION(BlueprintPure, Category = "Torch")
	float GetBatteryPercent() const { return MaxBattery > 0.f ? Battery / MaxBattery : 0.f; }

	UFUNCTION(BlueprintPure, Category = "Torch")
	bool IsBatteryLow() const { return GetBatteryPercent() <= LowBatteryPercent; }

	UPROPERTY(BlueprintAssignable, Category = "Torch")
	FOnTorchBatteryEmpty OnBatteryEmpty;

	/** Full brightness, in candelas. Tune this together with level exposure. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torch")
	float BaseIntensity = 200.f;

	/** BaseIntensity x this = the light's (unitless, radial falloff) brightness. Roughly lux on a wall 1-3 m away. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torch")
	float IntensityScale = 0.014f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torch|Battery")
	float MaxBattery = 100.f;

	/** 100 / 0.4 = 250 seconds of light from a full battery. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torch|Battery")
	float DrainPerSecond = 0.4f;

	/** Below this fraction (0-1) the torch dims and flickers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torch|Battery")
	float LowBatteryPercent = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torch")
	bool bStartOn = true;

protected:
	virtual void BeginPlay() override;

private:
	void ApplyLight(float Factor, bool bShine);

	float Battery = 100.f;
	bool bIsOn = false;
	float KillTimeRemaining = 0.f;

	float FlickerTimer = 0.f;
	float FlickerValue = 1.f;
	float LastFactor = -1.f;
};
