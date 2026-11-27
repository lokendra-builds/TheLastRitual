#include "TorchComponent.h"

UTorchComponent::UTorchComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Not physically inverse-squared: a real 200 cd torch blows a wall at 1 m out to white.
	// A radial falloff keeps the first few metres evenly readable and lets the far end of a
	// room sink into darkness. BaseIntensity stays in "candela" for callers; see GetLightScale().
	bUseInverseSquaredFalloff = false;
	LightFalloffExponent = 5.f;
	IntensityUnits = ELightUnits::Unitless;
	Intensity = BaseIntensity * IntensityScale;
	LightColor = FColor(255, 232, 196); // warm, old-bulb white
	InnerConeAngle = 5.f;
	OuterConeAngle = 30.f; // small hot spot, long soft spill
	AttenuationRadius = 1500.f;
	SourceRadius = 3.f; // softer shadow edges than a pin-point light
	SoftSourceRadius = 0.f;
	CastShadows = true;
	SpecularScale = 0.6f; // old bulb, no harsh glints
	VolumetricScatteringIntensity = 2.5f; // dust in the beam (volumetric fog)
}

void UTorchComponent::BeginPlay()
{
	Super::BeginPlay();

	Battery = MaxBattery;
	bIsOn = bStartOn;
	ApplyLight(1.f, bIsOn);
}

void UTorchComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (KillTimeRemaining > 0.f)
	{
		KillTimeRemaining = FMath::Max(0.f, KillTimeRemaining - DeltaTime);
	}

	if (bIsOn && Battery > 0.f)
	{
		Battery = FMath::Max(0.f, Battery - DrainPerSecond * DeltaTime);
		if (Battery <= 0.f)
		{
			bIsOn = false;
			OnBatteryEmpty.Broadcast();
		}
	}

	const bool bShine = bIsOn && Battery > 0.f && KillTimeRemaining <= 0.f;

	float Factor = 1.f;
	if (bShine && IsBatteryLow())
	{
		// Dim towards 40% as the battery runs out, plus random flicker.
		const float LowAlpha = LowBatteryPercent > 0.f ? GetBatteryPercent() / LowBatteryPercent : 0.f;
		const float Dim = FMath::Lerp(0.4f, 1.f, LowAlpha);

		FlickerTimer -= DeltaTime;
		if (FlickerTimer <= 0.f)
		{
			// Mostly steady, sometimes a sharp drop. Gets worse as battery falls.
			const float DropChance = FMath::Lerp(0.35f, 0.08f, LowAlpha);
			FlickerValue = (FMath::FRand() < DropChance) ? FMath::FRandRange(0.f, 0.3f) : FMath::FRandRange(0.85f, 1.f);
			FlickerTimer = FMath::FRandRange(0.04f, 0.25f);
		}
		Factor = Dim * FlickerValue;
	}

	ApplyLight(Factor, bShine);
}

void UTorchComponent::ApplyLight(float Factor, bool bShine)
{
	SetVisibility(bShine);

	if (bShine)
	{
		// Other code may set Intensity = BaseIntensity directly (gadget switch), so compare
		// against the actual value, not only the last factor.
		const float Wanted = BaseIntensity * Factor * IntensityScale;
		if (!FMath::IsNearlyEqual(Factor, LastFactor, 0.01f) || !FMath::IsNearlyEqual(Intensity, Wanted, 0.01f))
		{
			SetIntensity(Wanted);
			LastFactor = Factor;
		}
	}
}

void UTorchComponent::ToggleTorch()
{
	SetTorchOn(!bIsOn);
}

void UTorchComponent::SetTorchOn(bool bOn)
{
	// A dead torch just clicks. (Click sound comes in week 4.)
	bIsOn = bOn && Battery > 0.f;
}

void UTorchComponent::AddBattery(float Amount)
{
	Battery = FMath::Clamp(Battery + Amount, 0.f, MaxBattery);
}

void UTorchComponent::KillTorch(float Seconds)
{
	KillTimeRemaining = FMath::Max(KillTimeRemaining, Seconds);
}
