#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HorrorDirector.generated.h"

/**
 * Decides when something scary happens, based on the ghost's anger.
 *
 *   Calm      torch flickers for a moment
 *   Restless  an open door slams, or the torch dies for a second or two
 *   Angry+    the ghost shows itself at one of its appear points
 *
 * Settings come from the NightSetup actor. Sounds (whispers, footsteps)
 * hook in here during the sound pass.
 */
UCLASS()
class THELASTRITUAL_API UHorrorDirector : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Fire a scare right now (for testing or scripted moments). */
	UFUNCTION(BlueprintCallable, Category = "Horror Director")
	void TriggerEvent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Horror Director")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Horror Director")
	float RestlessAfterSeconds = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Horror Director")
	FVector2D CalmEventInterval = FVector2D(40.f, 80.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Horror Director")
	FVector2D RestlessEventInterval = FVector2D(20.f, 40.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Horror Director")
	FVector2D AngryEventInterval = FVector2D(10.f, 22.f);

	// ---- Hunts: the ghost suddenly comes for the player.

	/** No hunts before this many seconds into the night. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Horror Director|Hunt")
	float FirstHuntAfterSeconds = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Horror Director|Hunt")
	float MinSecondsBetweenHunts = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Horror Director|Hunt")
	float HuntDuration = 25.f;

	/** Chance that a scare event becomes a hunt, per anger level: Calm, Restless, Angry, Furious. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Horror Director|Hunt")
	FVector4 HuntChance = FVector4(0.1f, 0.3f, 0.45f, 0.6f);

	/** Start a hunt now if the ghost can (testing / scripted moments). */
	UFUNCTION(BlueprintCallable, Category = "Horror Director|Hunt")
	bool TryStartHunt();

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void ScheduleNext();

	class AGhostBase* FindGhost() const;

	float TimeUntilEvent = -1.f;
	float LastHuntEndTime = -1000.f;
};
