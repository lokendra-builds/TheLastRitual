#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Gadgets.h"
#include "ProgressSubsystem.generated.h"

UCLASS()
class THELASTRITUAL_API ULastRitualSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int64 Points = 0;

	UPROPERTY()
	TArray<uint8> OwnedGadgets;

	UPROPERTY()
	int32 NightsWon = 0;
};

/**
 * Points and owned gadgets, saved to disk (slot "Progress") so they survive
 * restarts and new nights. A new player starts with StartingPoints.
 */
UCLASS()
class THELASTRITUAL_API UProgressSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static constexpr int64 StartingPoints = 100000;
	static constexpr int64 PointsPerNightWon = 5000;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintPure, Category = "Progress")
	int64 GetPoints() const { return Save ? Save->Points : 0; }

	UFUNCTION(BlueprintPure, Category = "Progress")
	bool Owns(EGadget Gadget) const;

	/** Spend points on a gadget. False if already owned or not enough points. */
	UFUNCTION(BlueprintCallable, Category = "Progress")
	bool Buy(EGadget Gadget);

	UFUNCTION(BlueprintCallable, Category = "Progress")
	void AddPoints(int64 Amount);

	UFUNCTION(BlueprintCallable, Category = "Progress")
	void RecordNightWon();

	/** Back to a brand-new player (testing). */
	UFUNCTION(BlueprintCallable, Category = "Progress")
	void ResetProgress();

private:
	void WriteSave();

	UPROPERTY()
	TObjectPtr<ULastRitualSave> Save;
};
