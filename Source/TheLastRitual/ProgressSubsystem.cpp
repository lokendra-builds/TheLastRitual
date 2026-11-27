#include "ProgressSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	const TCHAR* SlotName = TEXT("Progress");
}

void UProgressSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// -FreshProgress: start as a new player (testing).
	if (!FParse::Param(FCommandLine::Get(), TEXT("FreshProgress")) && UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		Save = Cast<ULastRitualSave>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	}
	if (!Save)
	{
		ResetProgress();
	}
	UE_LOG(LogTemp, Log, TEXT("Progress: %lld points, %d gadgets owned"), Save->Points, Save->OwnedGadgets.Num());
}

bool UProgressSubsystem::Owns(EGadget Gadget) const
{
	static const bool bAll = FParse::Param(FCommandLine::Get(), TEXT("AllGadgets")); // testing
	return bAll || GadgetTable::Info(Gadget).Price == 0 || (Save && Save->OwnedGadgets.Contains(static_cast<uint8>(Gadget)));
}

bool UProgressSubsystem::Buy(EGadget Gadget)
{
	const int32 Price = GadgetTable::Info(Gadget).Price;
	if (!Save || Owns(Gadget) || Save->Points < Price)
	{
		return false;
	}
	Save->Points -= Price;
	Save->OwnedGadgets.AddUnique(static_cast<uint8>(Gadget));
	WriteSave();
	UE_LOG(LogTemp, Log, TEXT("Bought %s for %d. Points left: %lld"), GadgetTable::Info(Gadget).Name, Price, Save->Points);
	return true;
}

void UProgressSubsystem::AddPoints(int64 Amount)
{
	if (Save)
	{
		Save->Points = FMath::Max<int64>(0, Save->Points + Amount);
		WriteSave();
	}
}

void UProgressSubsystem::RecordNightWon()
{
	if (Save)
	{
		++Save->NightsWon;
		AddPoints(PointsPerNightWon);
	}
}

void UProgressSubsystem::ResetProgress()
{
	Save = Cast<ULastRitualSave>(UGameplayStatics::CreateSaveGameObject(ULastRitualSave::StaticClass()));
	Save->Points = StartingPoints;
	Save->OwnedGadgets.Reset();
	WriteSave();
}

void UProgressSubsystem::WriteSave()
{
	// -FreshProgress (tests) never writes, so it can't wipe the player's real save.
	static const bool bTestRun = FParse::Param(FCommandLine::Get(), TEXT("FreshProgress"));
	if (Save && !bTestRun)
	{
		UGameplayStatics::SaveGameToSlot(Save, SlotName, 0);
	}
}
