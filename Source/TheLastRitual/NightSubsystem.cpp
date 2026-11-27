#include "NightSubsystem.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "GhostDefinition.h"
#include "Engine/GameInstance.h"
#include "ProgressSubsystem.h"

bool UNightSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UNightSubsystem::StartNight(UGhostDefinition* Ghost, int32 StartingLives)
{
	TonightGhost = Ghost;
	MaxLives = FMath::Max(1, StartingLives);
	Lives = MaxLives;
	Anger = EGhostAnger::Calm;
	NightStartTime = GetWorld()->GetTimeSeconds();

	UE_LOG(LogTemp, Log, TEXT("Night started. Ghost: %s, lives: %d"),
		Ghost ? *Ghost->GetName() : TEXT("NONE (set it on the NightSetup actor)"), Lives);

	OnAngerChanged.Broadcast(Anger);
	OnLivesChanged.Broadcast(Lives);
	SetState(ENightState::Playing);
}

bool UNightSubsystem::PerformRitual(FName RitualId)
{
	if (State != ENightState::Playing)
	{
		return false;
	}

	const bool bCorrect = TonightGhost && TonightGhost->RitualId == RitualId;
	UE_LOG(LogTemp, Log, TEXT("Ritual %s: %s"), *RitualId.ToString(), bCorrect ? TEXT("correct") : TEXT("WRONG"));

	OnRitualPerformed.Broadcast(RitualId, bCorrect);

	if (bCorrect)
	{
		if (UProgressSubsystem* Progress = GetWorld()->GetGameInstance()->GetSubsystem<UProgressSubsystem>())
		{
			Progress->RecordNightWon();
		}
		SetState(ENightState::Won);
	}
	else
	{
		LoseLife();
	}
	return bCorrect;
}

void UNightSubsystem::LoseLife()
{
	if (State != ENightState::Playing)
	{
		return;
	}

	Lives = FMath::Max(0, Lives - 1);
	OnLivesChanged.Broadcast(Lives);

	if (Lives <= 0)
	{
		SetState(ENightState::Lost);
		return;
	}

	RaiseAngerTo(Lives == 1 ? EGhostAnger::Furious : EGhostAnger::Angry);
}

void UNightSubsystem::RaiseAngerTo(EGhostAnger NewAnger)
{
	if (NewAnger > Anger)
	{
		Anger = NewAnger;
		UE_LOG(LogTemp, Log, TEXT("Ghost anger: %d"), static_cast<int32>(Anger));
		OnAngerChanged.Broadcast(Anger);
	}
}

void UNightSubsystem::SetState(ENightState NewState)
{
	if (State == NewState)
	{
		return;
	}
	State = NewState;
	OnNightStateChanged.Broadcast(State);

	if (State == ENightState::Lost)
	{
		// Design: lose all lives -> restart the night. (Clue reshuffle comes later.)
		GetWorld()->GetTimerManager().SetTimer(RestartTimer,
			FTimerDelegate::CreateUObject(this, &UNightSubsystem::RestartNight), RestartDelay, false);
	}
}

void UNightSubsystem::RestartNight()
{
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}

float UNightSubsystem::GetNightTime() const
{
	return State == ENightState::NotStarted ? 0.f : GetWorld()->GetTimeSeconds() - NightStartTime;
}
