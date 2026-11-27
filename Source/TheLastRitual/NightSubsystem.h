#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/TimerHandle.h"
#include "NightSubsystem.generated.h"

class UGhostDefinition;

UENUM(BlueprintType)
enum class EGhostAnger : uint8
{
	Calm,      // whispers, small flickers
	Restless,  // doors slam, torch dies briefly
	Angry,     // after a wrong ritual: the ghost shows itself
	Furious    // last life
};

UENUM(BlueprintType)
enum class ENightState : uint8
{
	NotStarted,
	Playing,
	Won,
	Lost
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAngerChanged, EGhostAnger, NewAnger);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLivesChanged, int32, LivesLeft);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNightStateChanged, ENightState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRitualPerformed, FName, RitualId, bool, bCorrect);

/**
 * The rules of one night: tonight's ghost, lives, anger, and judging rituals.
 *
 *   right ritual  -> night won
 *   wrong ritual  -> lose a life, ghost gets Angry (Furious on the last life), jump scare
 *   no lives left -> night lost, level restarts
 *
 * Started by an ANightSetup actor placed in the level.
 */
UCLASS()
class THELASTRITUAL_API UNightSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Night")
	void StartNight(UGhostDefinition* Ghost, int32 StartingLives = 3);

	/** Judge a ritual against tonight's ghost. Returns true if it was the right one. */
	UFUNCTION(BlueprintCallable, Category = "Night")
	bool PerformRitual(FName RitualId);

	UFUNCTION(BlueprintCallable, Category = "Night")
	void LoseLife();

	/** Anger only ever goes up during a night. */
	UFUNCTION(BlueprintCallable, Category = "Night")
	void RaiseAngerTo(EGhostAnger NewAnger);

	UFUNCTION(BlueprintCallable, Category = "Night")
	void RestartNight();

	UFUNCTION(BlueprintPure, Category = "Night")
	UGhostDefinition* GetTonightGhost() const { return TonightGhost; }

	UFUNCTION(BlueprintPure, Category = "Night")
	int32 GetLives() const { return Lives; }

	UFUNCTION(BlueprintPure, Category = "Night")
	int32 GetMaxLives() const { return MaxLives; }

	UFUNCTION(BlueprintPure, Category = "Night")
	EGhostAnger GetAnger() const { return Anger; }

	UFUNCTION(BlueprintPure, Category = "Night")
	ENightState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Night")
	bool IsPlaying() const { return State == ENightState::Playing; }

	/** Seconds since the night started. */
	UFUNCTION(BlueprintPure, Category = "Night")
	float GetNightTime() const;

	UPROPERTY(BlueprintAssignable, Category = "Night")
	FOnAngerChanged OnAngerChanged;

	UPROPERTY(BlueprintAssignable, Category = "Night")
	FOnLivesChanged OnLivesChanged;

	UPROPERTY(BlueprintAssignable, Category = "Night")
	FOnNightStateChanged OnNightStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Night")
	FOnRitualPerformed OnRitualPerformed;

	/** Seconds between losing the night and the restart. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night")
	float RestartDelay = 6.f;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void SetState(ENightState NewState);

	UPROPERTY()
	TObjectPtr<UGhostDefinition> TonightGhost;

	int32 Lives = 3;
	int32 MaxLives = 3;
	EGhostAnger Anger = EGhostAnger::Calm;
	ENightState State = ENightState::NotStarted;
	float NightStartTime = 0.f;
	FTimerHandle RestartTimer;
};
