#include "HorrorDirector.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GhostBase.h"
#include "GhostHunterCharacter.h"
#include "HouseDoor.h"
#include "NightSubsystem.h"
#include "TorchComponent.h"

bool UHorrorDirector::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UHorrorDirector::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UHorrorDirector, STATGROUP_Tickables);
}

void UHorrorDirector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UNightSubsystem* Night = GetWorld()->GetSubsystem<UNightSubsystem>();
	if (!bEnabled || !Night || !Night->IsPlaying())
	{
		return;
	}

	if (Night->GetNightTime() >= RestlessAfterSeconds)
	{
		Night->RaiseAngerTo(EGhostAnger::Restless);
	}

	if (TimeUntilEvent < 0.f)
	{
		ScheduleNext();
		return;
	}

	TimeUntilEvent -= DeltaTime;
	if (TimeUntilEvent <= 0.f)
	{
		TriggerEvent();
		ScheduleNext();
	}
}

void UHorrorDirector::ScheduleNext()
{
	const UNightSubsystem* Night = GetWorld()->GetSubsystem<UNightSubsystem>();
	const EGhostAnger Anger = Night ? Night->GetAnger() : EGhostAnger::Calm;

	const FVector2D& Range =
		Anger == EGhostAnger::Calm ? CalmEventInterval :
		Anger == EGhostAnger::Restless ? RestlessEventInterval :
		AngryEventInterval;

	TimeUntilEvent = FMath::FRandRange(Range.X, Range.Y);
}

AGhostBase* UHorrorDirector::FindGhost() const
{
	TActorIterator<AGhostBase> It(GetWorld());
	return It ? *It : nullptr;
}

bool UHorrorDirector::TryStartHunt()
{
	AGhostBase* Ghost = FindGhost();
	if (!Ghost || Ghost->IsHunting() || Ghost->IsBanished())
	{
		return false;
	}
	Ghost->StartHunt(HuntDuration);
	return Ghost->IsHunting();
}

void UHorrorDirector::TriggerEvent()
{
	UWorld* World = GetWorld();
	const UNightSubsystem* Night = World->GetSubsystem<UNightSubsystem>();
	const EGhostAnger Anger = Night ? Night->GetAnger() : EGhostAnger::Calm;
	UE_LOG(LogTemp, Log, TEXT("Horror event: night time %.1fs, real time %.1fs, anger %d"),
		Night ? Night->GetNightTime() : 0.f, World->GetRealTimeSeconds(), static_cast<int32>(Anger));

	// A hunt in progress is scary enough; don't stack events on top of it.
	AGhostBase* Ghost = FindGhost();
	const float Now = World->GetTimeSeconds();
	if (Ghost && Ghost->IsHunting())
	{
		LastHuntEndTime = Now;
		return;
	}

	// Sometimes, out of nowhere: a hunt.
	const float NightTime = Night ? Night->GetNightTime() : 0.f;
	const float Chance = HuntChance[FMath::Clamp(static_cast<int32>(Anger), 0, 3)];
	if (NightTime >= FirstHuntAfterSeconds && Now - LastHuntEndTime >= MinSecondsBetweenHunts && FMath::FRand() < Chance)
	{
		if (TryStartHunt())
		{
			LastHuntEndTime = Now + HuntDuration;
			return;
		}
	}

	AGhostHunterCharacter* Hunter = Cast<AGhostHunterCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0));
	UTorchComponent* Torch = Hunter ? Hunter->GetTorch() : nullptr;

	switch (Anger)
	{
	case EGhostAnger::Calm:
		if (Torch)
		{
			Torch->KillTorch(0.25f);
		}
		UE_LOG(LogTemp, Log, TEXT("Horror: flicker"));
		break;

	case EGhostAnger::Restless:
	{
		TArray<AHouseDoor*> OpenDoors;
		for (TActorIterator<AHouseDoor> It(World); It; ++It)
		{
			if (It->IsOpen())
			{
				OpenDoors.Add(*It);
			}
		}

		if (OpenDoors.Num() > 0 && FMath::RandBool())
		{
			OpenDoors[FMath::RandRange(0, OpenDoors.Num() - 1)]->SetOpen(false, false, /*bSlam*/ true);
			UE_LOG(LogTemp, Log, TEXT("Horror: door slam"));
		}
		else if (Torch)
		{
			Torch->KillTorch(FMath::FRandRange(1.f, 2.f));
			UE_LOG(LogTemp, Log, TEXT("Horror: torch dies"));
		}
		break;
	}

	case EGhostAnger::Angry:
	case EGhostAnger::Furious:
	{
		if (Ghost)
		{
			Ghost->AppearAtRandomPoint(Anger == EGhostAnger::Furious ? 3.5f : 2.f);
		}
		if (Torch)
		{
			Torch->KillTorch(0.4f);
		}
		UE_LOG(LogTemp, Log, TEXT("Horror: ghost appears"));
		break;
	}
	}
}
