#pragma once

#include "CoreMinimal.h"
#include "Gadgets.generated.h"

/** Every gadget Arjun can carry. Number keys 1-5 select them in this order. */
UENUM(BlueprintType)
enum class EGadget : uint8
{
	Torch,
	GunTorch,
	UVTorch,
	Thermometer,
	EMFMeter,
	Count UMETA(Hidden)
};

struct FGadgetInfo
{
	EGadget Id;
	const TCHAR* Name;
	const TCHAR* Description;
	int32 Price;        // 0 = owned from the start
	FLinearColor Color; // icon tint
};

namespace GadgetTable
{
	inline const FGadgetInfo& Info(EGadget G)
	{
		static const FGadgetInfo Table[] = {
			{ EGadget::Torch,       TEXT("Torch"),       TEXT("Grandfather's old torch. Limited battery."), 0, FLinearColor(0.95f, 0.85f, 0.6f) },
			{ EGadget::GunTorch,    TEXT("Gun Torch"),   TEXT("Left click fires a blessed flash. Hurts spirits and drives them off. 12 shots a night."), 1500, FLinearColor(0.95f, 0.55f, 0.25f) },
			{ EGadget::UVTorch,     TEXT("UV Torch"),    TEXT("Shows handprints and writing the eye can't see."), 1000, FLinearColor(0.6f, 0.35f, 1.f) },
			{ EGadget::Thermometer, TEXT("Thermometer"), TEXT("Spirits steal the heat from a room. Watch for cold spots."), 1000, FLinearColor(0.45f, 0.8f, 1.f) },
			{ EGadget::EMFMeter,    TEXT("EMF Meter"),   TEXT("Beeps faster near spirit activity. Levels 1-5."), 1500, FLinearColor(0.4f, 1.f, 0.45f) },
		};
		return Table[FMath::Clamp(static_cast<int32>(G), 0, static_cast<int32>(EGadget::Count) - 1)];
	}

	constexpr int32 Num() { return static_cast<int32>(EGadget::Count); }
}
