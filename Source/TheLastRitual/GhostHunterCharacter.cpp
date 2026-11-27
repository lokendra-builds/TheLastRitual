#include "GhostHunterCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputActionValue.h"

#include "TorchComponent.h"
#include "Interactable.h"
#include "GhostHuntHUD.h"
#include "Components/AudioComponent.h"
#include "EngineUtils.h"
#include "GhostBase.h"
#include "HidingSpot.h"
#include "GadgetComponent.h"
#include "RecordingSubsystem.h"
#include "ViewModelComponent.h"
#include "HorrorSynthWave.h"
#include "NightSubsystem.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

AGhostHunterCharacter::AGhostHunterCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
	BaseEyeHeight = 70.f;
	CrouchedEyeHeight = 40.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.f, 0.f, BaseEyeHeight));
	Camera->bUsePawnControlRotation = true;
	Camera->SetFieldOfView(85.f);

	Torch = CreateDefaultSubobject<UTorchComponent>(TEXT("Torch"));
	Torch->SetupAttachment(Camera);
	// Held slightly right and below the eyes, like a real hand-held torch.
	Torch->SetRelativeLocation(FVector(10.f, 18.f, -14.f));

	Gadgets = CreateDefaultSubobject<UGadgetComponent>(TEXT("Gadgets"));
	ViewModel = CreateDefaultSubobject<UViewModelComponent>(TEXT("ViewModel"));

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = WalkSpeed;
	Move->MaxWalkSpeedCrouched = CrouchSpeed;
	Move->GetNavAgentPropertiesRef().bCanCrouch = true;
	Move->BrakingDecelerationWalking = 1400.f;
}

// ---------------------------------------------------------------- Input setup

void AGhostHunterCharacter::BuildDefaultInput()
{
	if (MappingContext)
	{
		return; // A Blueprint child supplied its own assets.
	}

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_GhostHunter"));

	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = Type;
		return Action;
	};

	MoveAction     = MakeAction(TEXT("IA_Move"),     EInputActionValueType::Axis2D);
	LookAction     = MakeAction(TEXT("IA_Look"),     EInputActionValueType::Axis2D);
	RunAction      = MakeAction(TEXT("IA_Run"),      EInputActionValueType::Boolean);
	CrouchAction   = MakeAction(TEXT("IA_Crouch"),   EInputActionValueType::Boolean);
	TorchAction    = MakeAction(TEXT("IA_Torch"),    EInputActionValueType::Boolean);
	InteractAction = MakeAction(TEXT("IA_Interact"), EInputActionValueType::Boolean);
	GuideAction    = MakeAction(TEXT("IA_Guide"),    EInputActionValueType::Boolean);
	MenuAction     = MakeAction(TEXT("IA_Menu"),     EInputActionValueType::Boolean);
	MenuAction->bTriggerWhenPaused = true; // Esc must work while the menu has paused the game
	ShopAction     = MakeAction(TEXT("IA_Shop"),     EInputActionValueType::Boolean);
	ShopAction->bTriggerWhenPaused = true;
	MapAction      = MakeAction(TEXT("IA_Map"),      EInputActionValueType::Boolean);
	UseAction      = MakeAction(TEXT("IA_Use"),      EInputActionValueType::Boolean);
	ControlsAction = MakeAction(TEXT("IA_Controls"), EInputActionValueType::Boolean);
	RecordAction   = MakeAction(TEXT("IA_Record"),   EInputActionValueType::Boolean);
	RecordAction->bTriggerWhenPaused = true;
	GadgetSlotActions.Reset();
	for (int32 i = 0; i < GadgetTable::Num(); ++i)
	{
		GadgetSlotActions.Add(MakeAction(*FString::Printf(TEXT("IA_Slot%d"), i + 1), EInputActionValueType::Boolean));
	}

	// WASD -> 2D axis. X = right, Y = forward.
	{
		FEnhancedActionKeyMapping& W = MappingContext->MapKey(MoveAction, EKeys::W);
		W.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this)); // default order YXZ: key value goes to Y
	}
	{
		FEnhancedActionKeyMapping& S = MappingContext->MapKey(MoveAction, EKeys::S);
		S.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
		S.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}
	{
		FEnhancedActionKeyMapping& A = MappingContext->MapKey(MoveAction, EKeys::A);
		A.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}
	MappingContext->MapKey(MoveAction, EKeys::D);

	// Mouse look, Y inverted so moving the mouse up looks up.
	{
		FEnhancedActionKeyMapping& Mouse = MappingContext->MapKey(LookAction, EKeys::Mouse2D);
		UInputModifierNegate* NegateY = NewObject<UInputModifierNegate>(this);
		NegateY->bX = false;
		NegateY->bY = true;
		NegateY->bZ = false;
		Mouse.Modifiers.Add(NegateY);
	}

	MappingContext->MapKey(RunAction, EKeys::LeftShift);
	MappingContext->MapKey(CrouchAction, EKeys::C);
	MappingContext->MapKey(TorchAction, EKeys::F);
	MappingContext->MapKey(InteractAction, EKeys::E);
	MappingContext->MapKey(GuideAction, EKeys::Tab);
	MappingContext->MapKey(MenuAction, EKeys::Escape);
	MappingContext->MapKey(ShopAction, EKeys::B);
	MappingContext->MapKey(MapAction, EKeys::M);
	MappingContext->MapKey(UseAction, EKeys::LeftMouseButton);
	MappingContext->MapKey(ControlsAction, EKeys::H);
	MappingContext->MapKey(RecordAction, EKeys::F9);
	const FKey SlotKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five };
	for (int32 i = 0; i < GadgetSlotActions.Num() && i < UE_ARRAY_COUNT(SlotKeys); ++i)
	{
		MappingContext->MapKey(GadgetSlotActions[i], SlotKeys[i]);
	}
}

void AGhostHunterCharacter::AddMappingContext()
{
	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			BuildDefaultInput();
			Subsystem->AddMappingContext(MappingContext, 0);
		}
	}
}

void AGhostHunterCharacter::RemoveMappingContext()
{
	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (MappingContext)
			{
				Subsystem->RemoveMappingContext(MappingContext);
			}
		}
	}
}

void AGhostHunterCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocallyControlled() || GetNetMode() == NM_Standalone)
	{
		HorrorWave = NewObject<UHorrorSynthWave>(this);
		HorrorAudio = NewObject<UAudioComponent>(this, TEXT("HorrorAudio"));
		HorrorAudio->bAutoActivate = false;
		HorrorAudio->bIsUISound = false;
		HorrorAudio->bAllowSpatialization = false;
		HorrorAudio->SetupAttachment(RootComponent);
		HorrorAudio->RegisterComponent();
		HorrorAudio->SetSound(HorrorWave);
		HorrorAudio->Play();

		// Second, effects-only synth for UI sounds: a UI sound keeps playing while the menu pauses the game.
		UiWave = NewObject<UHorrorSynthWave>(this);
		UiWave->SetSfxOnly(true);
		UiAudio = NewObject<UAudioComponent>(this, TEXT("UiAudio"));
		UiAudio->bAutoActivate = false;
		UiAudio->bIsUISound = true;
		UiAudio->bAllowSpatialization = false;
		UiAudio->SetupAttachment(RootComponent);
		UiAudio->RegisterComponent();
		UiAudio->SetSound(UiWave);
		UiAudio->Play();
	}

	// Test option: -DumpAudio=<music seconds> renders every synthesised sound to Saved/AudioTest/SynthTest.wav.
	float DumpSeconds = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("DumpAudio="), DumpSeconds) && DumpSeconds > 0.f)
	{
		UHorrorSynthWave::RenderTestWav(FPaths::ProjectSavedDir() / TEXT("AudioTest/SynthTest.wav"), FMath::Min(DumpSeconds, 600.f));
	}
}

void AGhostHunterCharacter::PlayUiSound(bool bPurchase)
{
	if (UHorrorSynthWave* Synth = UiWave ? UiWave.Get() : HorrorWave.Get())
	{
		Synth->PlaySfx(bPurchase ? EHorrorSfx::Purchase : EHorrorSfx::UIClick);
	}
}

void AGhostHunterCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	AddMappingContext();
}

void AGhostHunterCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Restarting a night spawns a new hunter; don't leave our keys mapped twice.
	RemoveMappingContext();
	Super::EndPlay(EndPlayReason);
}

void AGhostHunterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	BuildDefaultInput();

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		UE_LOG(LogTemp, Error, TEXT("GhostHunter: Enhanced Input is not the default input component. Check Config/DefaultInput.ini."));
		return;
	}

	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AGhostHunterCharacter::Move);
	Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &AGhostHunterCharacter::Look);
	Input->BindAction(RunAction, ETriggerEvent::Started, this, &AGhostHunterCharacter::StartRun);
	Input->BindAction(RunAction, ETriggerEvent::Completed, this, &AGhostHunterCharacter::StopRun);
	Input->BindAction(CrouchAction, ETriggerEvent::Started, this, &AGhostHunterCharacter::ToggleCrouch);
	Input->BindAction(TorchAction, ETriggerEvent::Started, this, &AGhostHunterCharacter::ToggleTorch);
	Input->BindAction(InteractAction, ETriggerEvent::Started, this, &AGhostHunterCharacter::Interact);
	Input->BindAction(GuideAction, ETriggerEvent::Started, this, &AGhostHunterCharacter::ToggleGuide);
	Input->BindAction(MenuAction, ETriggerEvent::Started, this, &AGhostHunterCharacter::ToggleMenu);
	Input->BindAction(ShopAction, ETriggerEvent::Started, this, &AGhostHunterCharacter::ToggleShop);
	Input->BindAction(MapAction, ETriggerEvent::Started, this, &AGhostHunterCharacter::ToggleMap);
	Input->BindAction(UseAction, ETriggerEvent::Started, this, &AGhostHunterCharacter::UseGadget);
	Input->BindAction(ControlsAction, ETriggerEvent::Started, this, &AGhostHunterCharacter::ToggleControls);
	Input->BindAction(RecordAction, ETriggerEvent::Started, this, &AGhostHunterCharacter::ToggleRecording);
	for (int32 i = 0; i < GadgetSlotActions.Num(); ++i)
	{
		Input->BindAction(GadgetSlotActions[i], ETriggerEvent::Started, this, &AGhostHunterCharacter::SelectSlot, i);
	}
}

// ---------------------------------------------------------------- Actions

void AGhostHunterCharacter::Move(const FInputActionValue& Value)
{
	if (IsHiding())
	{
		return;
	}
	const FVector2D Axis = Value.Get<FVector2D>();
	AddMovementInput(GetActorForwardVector(), Axis.Y);
	AddMovementInput(GetActorRightVector(), Axis.X);
}

void AGhostHunterCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X * LookSensitivity);
	AddControllerPitchInput(Axis.Y * LookSensitivity);
}

void AGhostHunterCharacter::StartRun()
{
	if (IsHiding())
	{
		return;
	}
	bWantsToRun = true;
	if (bIsCrouched)
	{
		UnCrouch();
	}
	UpdateSpeed();
}

void AGhostHunterCharacter::StopRun()
{
	bWantsToRun = false;
	UpdateSpeed();
}

void AGhostHunterCharacter::ToggleCrouch()
{
	if (IsHiding())
	{
		return;
	}
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		bWantsToRun = false;
		UpdateSpeed();
		Crouch();
	}
}

void AGhostHunterCharacter::UpdateSpeed()
{
	GetCharacterMovement()->MaxWalkSpeed = bWantsToRun ? RunSpeed : WalkSpeed;
}

void AGhostHunterCharacter::ToggleTorch()
{
	if (Torch)
	{
		Torch->ToggleTorch();
		if (HorrorWave)
		{
			HorrorWave->PlaySfx(EHorrorSfx::TorchClick);
		}
	}
}

void AGhostHunterCharacter::Interact()
{
	if (IsHiding())
	{
		ExitHiding();
		return;
	}
	if (AActor* Target = FocusedActor.Get())
	{
		IInteractable::Execute_Interact(Target, this);
	}
}

void AGhostHunterCharacter::ShowMessage(const FText& Message, float Seconds)
{
	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (AGhostHuntHUD* HUD = PC->GetHUD<AGhostHuntHUD>())
		{
			HUD->ShowMessage(Message, Seconds);
		}
	}
}

void AGhostHunterCharacter::ToggleMenu()
{
	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (AGhostHuntHUD* HUD = PC->GetHUD<AGhostHuntHUD>())
		{
			HUD->ToggleMenu();
		}
	}
}

void AGhostHunterCharacter::ToggleShop()
{
	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (AGhostHuntHUD* HUD = PC->GetHUD<AGhostHuntHUD>())
		{
			HUD->ToggleShop();
		}
	}
}

void AGhostHunterCharacter::ToggleMap()
{
	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (AGhostHuntHUD* HUD = PC->GetHUD<AGhostHuntHUD>())
		{
			HUD->ToggleMap();
		}
	}
}

void AGhostHunterCharacter::UseGadget()
{
	if (!Gadgets)
	{
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("Left click with %s"), GadgetTable::Info(Gadgets->GetEquipped()).Name);

	if (Gadgets->GetEquipped() != EGadget::GunTorch)
	{
		// Only the Gun Torch fires. Say how to get there instead of doing nothing.
		ShowMessage(Gadgets->IsOwned(EGadget::GunTorch)
			? NSLOCTEXT("GhostHunt", "PressTwo", "Press 2 to take out the Gun Torch, then left click to fire.")
			: NSLOCTEXT("GhostHunt", "BuyGun", "Left click fires the Gun Torch. You don't have it yet: press B and buy it (1,500), then press 2."), 4.f);
		return;
	}
	Gadgets->Use();
}

void AGhostHunterCharacter::ToggleRecording()
{
	if (URecordingSubsystem* Rec = GetGameInstance() ? GetGameInstance()->GetSubsystem<URecordingSubsystem>() : nullptr)
	{
		ShowMessage(Rec->Toggle(), 6.f);
	}
}

void AGhostHunterCharacter::ToggleControls()
{
	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (AGhostHuntHUD* HUD = PC->GetHUD<AGhostHuntHUD>())
		{
			HUD->ToggleControls();
		}
	}
}

void AGhostHunterCharacter::SelectSlot(int32 Slot)
{
	if (Gadgets && Slot >= 0 && Slot < GadgetTable::Num())
	{
		Gadgets->Equip(static_cast<EGadget>(Slot));
	}
}

void AGhostHunterCharacter::ToggleGuide()
{
	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (AGhostHuntHUD* HUD = PC->GetHUD<AGhostHuntHUD>())
		{
			HUD->ToggleGhostGuide();
		}
	}
}

// ---------------------------------------------------------------- Hiding

void AGhostHunterCharacter::EnterHiding(AHidingSpot* Spot)
{
	if (!Spot || IsHiding())
	{
		return;
	}
	if (bIsCrouched)
	{
		UnCrouch();
	}
	bWantsToRun = false;
	UpdateSpeed();

	HidingSpot = Spot;
	HideStartTime = GetWorld()->GetTimeSeconds();

	// A torch inside a cupboard is a beacon. Switch it off, remember the state.
	bTorchWasOn = Torch && Torch->IsTorchOn();
	if (Torch)
	{
		Torch->SetTorchOn(false);
	}

	GetCharacterMovement()->DisableMovement();
	SetActorEnableCollision(false);
	SetActorLocation(Spot->GetHideLocation(), false, nullptr, ETeleportType::TeleportPhysics);
	if (AController* C = GetController())
	{
		C->SetControlRotation(Spot->GetLookOutRotation());
	}
	FocusedActor = nullptr;
	if (HorrorWave)
	{
		HorrorWave->PlaySfx(EHorrorSfx::AlmirahIn);
	}
}

void AGhostHunterCharacter::ExitHiding()
{
	AHidingSpot* Spot = HidingSpot.Get();
	HidingSpot = nullptr;
	if (!Spot)
	{
		return;
	}

	SetActorLocation(Spot->GetExitLocation(), false, nullptr, ETeleportType::TeleportPhysics);
	SetActorEnableCollision(true);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	if (Torch && bTorchWasOn)
	{
		Torch->SetTorchOn(true);
	}
	StepDistance = 0.f;
	if (HorrorWave)
	{
		HorrorWave->PlaySfx(EHorrorSfx::AlmirahOut);
	}
}

float AGhostHunterCharacter::GetTimeHidden() const
{
	return IsHiding() ? GetWorld()->GetTimeSeconds() - HideStartTime : 0.f;
}

// ---------------------------------------------------------------- Sound

void AGhostHunterCharacter::UpdateHorrorAudio(float DeltaSeconds)
{
	if (!HorrorWave)
	{
		return;
	}

	TActorIterator<AGhostBase> It(GetWorld());
	const AGhostBase* Ghost = It ? *It : nullptr;
	const UNightSubsystem* Night = GetWorld()->GetSubsystem<UNightSubsystem>();
	const bool bPlaying = Night && Night->IsPlaying();

	float Closeness = 0.f; // 0 = far / not hunting, 1 = right here
	bool bWarning = false;
	bool bHunting = false;
	if (Ghost && bPlaying && Ghost->IsHunting())
	{
		bHunting = true;
		bWarning = Ghost->IsHuntWarning();
		if (!bWarning)
		{
			Closeness = FMath::GetMappedRangeValueClamped(FVector2D(2000.f, 150.f), FVector2D(0.f, 1.f),
				FVector::Dist2D(Ghost->GetActorLocation(), GetActorLocation()));
		}
	}

	const float Anger = Night ? static_cast<float>(Night->GetAnger()) / 3.f : 0.f;

	// Design: the house goes silent just before she comes.
	HorrorWave->SetAmbienceLevel(bWarning ? 0.f : (bHunting ? 0.25f : 0.55f + 0.2f * Anger));

	const float WarningRamp = bWarning ? Ghost->GetHuntWarningProgress() : 0.f;
	const float HeartLevel = bHunting ? FMath::Max(0.45f + 0.55f * Closeness, 0.3f + 0.5f * WarningRamp) : (IsHiding() ? 0.3f : 0.f);
	const float HeartBpm = 65.f + 90.f * FMath::Max(Closeness, 0.6f * WarningRamp) + (IsHiding() ? 15.f : 0.f);
	HorrorWave->SetHeartbeat(HeartBpm, HeartLevel);

	HorrorWave->SetWhisperLevel(bHunting && !bWarning ? FMath::Clamp(Closeness * 1.3f, 0.f, 1.f) : (bWarning ? 0.15f * WarningRamp : 0.f));

	// Music: soft tanpura + bansuri. The house goes quiet before she comes; hushed while hiding.
	HorrorWave->SetMusicLevel(bHunting ? 0.05f : (IsHiding() ? 0.15f : 0.35f));
}

void AGhostHunterCharacter::UpdateFootsteps(float DeltaSeconds)
{
	const UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!HorrorWave || !MoveComp || IsHiding() || !MoveComp->IsMovingOnGround())
	{
		StepDistance = 0.f;
		return;
	}

	const float Speed = GetVelocity().Size2D();
	if (Speed < 40.f)
	{
		// Standing still: the next step comes soon after setting off again.
		StepDistance = FMath::Min(StepDistance, 60.f);
		return;
	}

	// Seconds per step: crouch ~0.65, walk ~0.5, run ~0.33.
	const float Interval = Speed <= WalkSpeed
		? FMath::GetMappedRangeValueClamped(FVector2D(CrouchSpeed, WalkSpeed), FVector2D(0.65f, 0.5f), Speed)
		: FMath::GetMappedRangeValueClamped(FVector2D(WalkSpeed, RunSpeed), FVector2D(0.5f, 0.33f), Speed);
	const float Stride = Speed * Interval;

	StepDistance += Speed * DeltaSeconds;
	if (StepDistance >= Stride)
	{
		StepDistance = FMath::Fmod(StepDistance, Stride);
		const float Volume = bIsCrouched
			? 0.22f
			: FMath::GetMappedRangeValueClamped(FVector2D(WalkSpeed * 0.6f, RunSpeed), FVector2D(0.5f, 1.f), Speed);
		HorrorWave->PlaySfx(EHorrorSfx::Footstep, Volume);
	}
}

// ---------------------------------------------------------------- Focus

void AGhostHunterCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IsLocallyControlled())
	{
		if (IsHiding())
		{
			FocusedActor = nullptr;
		}
		else
		{
			UpdateFocus();
		}
		UpdateHorrorAudio(DeltaSeconds);
		UpdateFootsteps(DeltaSeconds);
	}
}

void AGhostHunterCharacter::UpdateFocus()
{
	const FVector Start = Camera->GetComponentLocation();
	const FVector End = Start + Camera->GetForwardVector() * InteractDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GhostHuntInteract), false, this);
	FHitResult Hit;

	AActor* NewFocus = nullptr;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		AActor* HitActor = Hit.GetActor();
		if (HitActor && HitActor->Implements<UInteractable>())
		{
			NewFocus = HitActor;
		}
	}

	FocusedActor = NewFocus;
}

FText AGhostHunterCharacter::GetFocusedPrompt() const
{
	if (IsHiding())
	{
		return NSLOCTEXT("GhostHunt", "LeaveHiding", "Get out");
	}
	if (AActor* Target = FocusedActor.Get())
	{
		return IInteractable::Execute_GetInteractPrompt(Target);
	}
	return FText::GetEmpty();
}
