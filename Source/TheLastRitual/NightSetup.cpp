#include "NightSetup.h"

#include "Components/BillboardComponent.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "GhostGuideSubsystem.h"
#include "HorrorDirector.h"
#include "NightSubsystem.h"

ANightSetup::ANightSetup()
{
	RootComponent = CreateDefaultSubobject<UBillboardComponent>(TEXT("Icon"));
}

void ANightSetup::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();

	if (UHorrorDirector* Director = World->GetSubsystem<UHorrorDirector>())
	{
		Director->RestlessAfterSeconds = RestlessAfterSeconds;
		Director->CalmEventInterval = CalmEventInterval;
		Director->RestlessEventInterval = RestlessEventInterval;
		Director->AngryEventInterval = AngryEventInterval;
		Director->FirstHuntAfterSeconds = FirstHuntAfterSeconds;
		Director->MinSecondsBetweenHunts = MinSecondsBetweenHunts;
		Director->HuntDuration = HuntDuration;
		Director->HuntChance = HuntChance;

		// Testing: -FastHunts makes the ghost hunt almost right away and often.
		if (FParse::Param(FCommandLine::Get(), TEXT("FastHunts")))
		{
			Director->CalmEventInterval = FVector2D(3.f, 5.f);
			Director->FirstHuntAfterSeconds = 4.f;
			Director->MinSecondsBetweenHunts = 3.f;
			Director->HuntChance = FVector4(1.f, 1.f, 1.f, 1.f);
		}
	}

	if (UGhostGuideSubsystem* Guide = World->GetSubsystem<UGhostGuideSubsystem>())
	{
		Guide->ResetNight();
	}

	if (UNightSubsystem* Night = World->GetSubsystem<UNightSubsystem>())
	{
		Night->StartNight(TonightGhost, StartingLives);
	}
}
