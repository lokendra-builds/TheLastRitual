#include "GhostHuntGameMode.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "GhostHunterCharacter.h"
#include "GhostHuntHUD.h"
#include "EngineUtils.h"
#include "HidingSpot.h"
#include "MirrorActor.h"
#include "Interactable.h"
#include "Camera/CameraComponent.h"
#include "GadgetComponent.h"
#include "GhostBase.h"
#include "RecordingSubsystem.h"

AGhostHuntGameMode::AGhostHuntGameMode()
{
	DefaultPawnClass = AGhostHunterCharacter::StaticClass();
	HUDClass = AGhostHuntHUD::StaticClass();
}

void AGhostHuntGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Testing: -AutoHide puts the player in the nearest almirah after a second.
	if (FParse::Param(FCommandLine::Get(), TEXT("AutoHide")))
	{
		FTimerHandle HideTimer;
		GetWorldTimerManager().SetTimer(HideTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			AGhostHunterCharacter* Hunter = Cast<AGhostHunterCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
			AHidingSpot* Nearest = nullptr;
			float Best = TNumericLimits<float>::Max();
			for (TActorIterator<AHidingSpot> It(GetWorld()); It && Hunter; ++It)
			{
				const float D = FVector::Dist(It->GetActorLocation(), Hunter->GetActorLocation());
				if (D < Best) { Best = D; Nearest = *It; }
			}
			if (Hunter && Nearest)
			{
				Hunter->EnterHiding(Nearest);
				UE_LOG(LogTemp, Log, TEXT("AutoHide: player hiding in %s"), *Nearest->GetActorNameOrLabel());
			}
		}), 1.f, false);
	}

	// Testing: -AutoFight=<gadget slot 1-5> equips it, aims at the ghost and uses it twice a second.
	int32 FightSlot = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("AutoFight="), FightSlot) && FightSlot > 0)
	{
		// Stand in the open courtyard so no closed door is between us and her.
		if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			Pawn->SetActorLocation(FVector(0.f, -350.f, Pawn->GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
		}
		FTimerHandle FightTimer;
		GetWorldTimerManager().SetTimer(FightTimer, FTimerDelegate::CreateWeakLambda(this, [this, FightSlot]()
		{
			AGhostHunterCharacter* Hunter = Cast<AGhostHunterCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
			UGadgetComponent* G = Hunter ? Hunter->GetGadgets() : nullptr;
			if (!G)
			{
				return;
			}
			if (G->GetEquipped() != static_cast<EGadget>(FightSlot - 1))
			{
				G->Equip(static_cast<EGadget>(FightSlot - 1));
			}
			TActorIterator<AGhostBase> It(GetWorld());
			if (It && It->CanBeHit() && Hunter->GetController())
			{
				const FVector Chest = It->GetActorLocation() + FVector(0.f, 0.f, 110.f);
				Hunter->GetController()->SetControlRotation((Chest - Hunter->GetCamera()->GetComponentLocation()).Rotation());
				G->Use();
			}
			if (G->GetEquipped() == EGadget::Thermometer || G->GetEquipped() == EGadget::EMFMeter)
			{
				UE_LOG(LogTemp, Log, TEXT("AutoFight reading: %.1f C, EMF %d"), G->GetTemperature(), G->GetEMFLevel());
			}
		}), 0.5f, true);
	}

	// Testing: -AutoRitual covers every mirror but one, examines the last and traps her in it.
	if (FParse::Param(FCommandLine::Get(), TEXT("AutoRitual")))
	{
		FTimerHandle RitualTimer;
		GetWorldTimerManager().SetTimer(RitualTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			AGhostHunterCharacter* Hunter = Cast<AGhostHunterCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
			TArray<AMirrorActor*> Mirrors;
			for (TActorIterator<AMirrorActor> It(GetWorld()); It; ++It)
			{
				Mirrors.Add(*It);
			}
			if (!Hunter || Mirrors.Num() < 2)
			{
				return;
			}
			for (int32 i = 1; i < Mirrors.Num(); ++i)
			{
				Mirrors[i]->SetCovered(true);
			}
			UE_LOG(LogTemp, Log, TEXT("AutoRitual: covered %d mirrors, using %s"), Mirrors.Num() - 1, *Mirrors[0]->GetActorNameOrLabel());
			IInteractable::Execute_Interact(Mirrors[0], Hunter); // examine (if it has a clue)
			IInteractable::Execute_Interact(Mirrors[0], Hunter); // trap her
		}), 2.f, false);
	}

	// Testing: -AutoFireAhead=<shots> fires the Gun Torch straight ahead, from 2 s, twice a second.
	int32 FireShots = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("AutoFireAhead="), FireShots) && FireShots > 0)
	{
		TSharedRef<int32> Left = MakeShared<int32>(FireShots);
		FTimerHandle FireTimer;
		GetWorldTimerManager().SetTimer(FireTimer, FTimerDelegate::CreateWeakLambda(this, [this, Left]()
		{
			AGhostHunterCharacter* Hunter = Cast<AGhostHunterCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
			UGadgetComponent* G = Hunter ? Hunter->GetGadgets() : nullptr;
			if (!G || *Left <= 0)
			{
				return;
			}
			if (G->GetEquipped() != EGadget::GunTorch)
			{
				G->Equip(EGadget::GunTorch);
			}
			// Aim where the screenshot will look (-AutoShotYaw / -AutoShotPitch), a little scattered.
			float Yaw = 0.f, Pitch = 0.f;
			if (Hunter->GetController() && FParse::Value(FCommandLine::Get(), TEXT("AutoShotYaw="), Yaw))
			{
				FParse::Value(FCommandLine::Get(), TEXT("AutoShotPitch="), Pitch);
				const float Scatter = FParse::Param(FCommandLine::Get(), TEXT("AutoFireExact")) ? 0.f : 1.f;
				Hunter->GetController()->SetControlRotation(FRotator(Pitch + Scatter * (FMath::FRandRange(-4.f, 4.f) - 6.f), Yaw + Scatter * FMath::FRandRange(-6.f, 6.f), 0.f));
			}
			G->Use();
			--(*Left);
		}), 0.5f, true, 2.f);
	}

	// Testing: -AutoRecord=<seconds> records that long after 2 s, then quits.
	float RecordSeconds = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("AutoRecord="), RecordSeconds) && RecordSeconds > 0.f)
	{
		FTimerHandle RecTimer;
		GetWorldTimerManager().SetTimer(RecTimer, FTimerDelegate::CreateWeakLambda(this, [this, RecordSeconds]()
		{
			URecordingSubsystem* Rec = GetGameInstance()->GetSubsystem<URecordingSubsystem>();
			UE_LOG(LogTemp, Log, TEXT("AutoRecord: %s"), *Rec->Start().ToString());
			FTimerHandle StopTimer;
			GetWorldTimerManager().SetTimer(StopTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				URecordingSubsystem* R = GetGameInstance()->GetSubsystem<URecordingSubsystem>();
				UE_LOG(LogTemp, Log, TEXT("AutoRecord: %s"), *R->Stop().ToString());
				QuitAfterShot();
			}), RecordSeconds, false);
		}), 2.f, false);
	}

	float Delay = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("AutoShot="), Delay) && Delay > 0.f)
	{
		GetWorldTimerManager().SetTimer(AutoShotTimer, this, &AGhostHuntGameMode::TakeAutoShot, Delay, false);
	}
}

void AGhostHuntGameMode::TakeAutoShot()
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		// Optional teleport: -AutoShotX= -AutoShotY= (cm, floor level).
		float X = 0.f, Y = 0.f;
		if (APawn* Pawn = PC->GetPawn())
		{
			if (FParse::Value(FCommandLine::Get(), TEXT("AutoShotX="), X) && FParse::Value(FCommandLine::Get(), TEXT("AutoShotY="), Y))
			{
				Pawn->SetActorLocation(FVector(X, Y, Pawn->GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
			}
		}

		float Yaw = 0.f, Pitch = 0.f;
		const bool bYaw = FParse::Value(FCommandLine::Get(), TEXT("AutoShotYaw="), Yaw);
		const bool bPitch = FParse::Value(FCommandLine::Get(), TEXT("AutoShotPitch="), Pitch);
		if (bYaw || bPitch)
		{
			FRotator Rot = PC->GetControlRotation();
			if (bYaw) Rot.Yaw = Yaw;
			if (bPitch) Rot.Pitch = Pitch;
			PC->SetControlRotation(Rot);
		}
	}

	// -AutoShotMenu: capture the Esc menu instead of gameplay. The menu pauses the
	// world (and its timers), so use the core ticker for the shot and the quit.
	const bool bShotMenu = FParse::Param(FCommandLine::Get(), TEXT("AutoShotMenu"));
	const bool bShotShop = FParse::Param(FCommandLine::Get(), TEXT("AutoShotShop"));
	if (bShotMenu || bShotShop)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			if (AGhostHuntHUD* HUD = PC->GetHUD<AGhostHuntHUD>())
			{
				if (bShotShop) HUD->ToggleShop(); else HUD->ToggleMenu();
			}
		}
		TWeakObjectPtr<AGhostHuntGameMode> WeakThis(this);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
		{
			if (WeakThis.IsValid())
			{
				UKismetSystemLibrary::ExecuteConsoleCommand(WeakThis.Get(), TEXT("HighResShot 1280x720"));
			}
			return false;
		}), 1.0f);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->QuitAfterShot();
			}
			return false;
		}), 2.5f);
		return;
	}

	// -AutoShotMapView: capture with the M map open.
	if (FParse::Param(FCommandLine::Get(), TEXT("AutoShotMapView")))
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			if (AGhostHuntHUD* HUD = PC->GetHUD<AGhostHuntHUD>())
			{
				HUD->ToggleMap();
			}
		}
	}

	// Give the new view a moment to settle, then capture and quit.
	FTimerHandle Shot;
	GetWorldTimerManager().SetTimer(Shot, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		UKismetSystemLibrary::ExecuteConsoleCommand(this, TEXT("HighResShot 1280x720"));
	}), 1.0f, false);
	GetWorldTimerManager().SetTimer(AutoShotTimer, this, &AGhostHuntGameMode::QuitAfterShot, 2.5f, false);
}

void AGhostHuntGameMode::QuitAfterShot()
{
	UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
}
