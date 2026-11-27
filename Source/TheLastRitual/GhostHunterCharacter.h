#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GhostHunterCharacter.generated.h"

class UCameraComponent;
class UTorchComponent;
class UInputAction;
class UInputMappingContext;
class UAudioComponent;
class UHorrorSynthWave;
class AHidingSpot;
class UGadgetComponent;
class UViewModelComponent;
struct FInputActionValue;

/**
 * Arjun. First-person player: walk, run, crouch, torch, interact.
 *
 * Input is built in C++ at runtime, so no input assets are needed. To use
 * your own Input Actions / Mapping Context from the editor instead, make a
 * Blueprint child of this class and fill in the Input slots.
 */
UCLASS()
class THELASTRITUAL_API AGhostHunterCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AGhostHunterCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void NotifyControllerChanged() override;

	UFUNCTION(BlueprintPure, Category = "Ghost Hunter")
	UTorchComponent* GetTorch() const { return Torch; }

	UFUNCTION(BlueprintPure, Category = "Ghost Hunter")
	UCameraComponent* GetCamera() const { return Camera; }

	UFUNCTION(BlueprintPure, Category = "Ghost Hunter")
	UGadgetComponent* GetGadgets() const { return Gadgets; }

	/** The interactable the player is looking at, or null. */
	UFUNCTION(BlueprintPure, Category = "Ghost Hunter")
	AActor* GetFocusedActor() const { return FocusedActor.Get(); }

	/** Prompt text of the focused interactable, or empty. */
	UFUNCTION(BlueprintPure, Category = "Ghost Hunter")
	FText GetFocusedPrompt() const;

	// ---- Ritual items (salt, iron nail, letter...). Simple named items, no inventory UI yet.

	UFUNCTION(BlueprintCallable, Category = "Ghost Hunter|Items")
	void AddItem(FName ItemId) { Items.Add(ItemId); }

	UFUNCTION(BlueprintCallable, Category = "Ghost Hunter|Items")
	bool RemoveItem(FName ItemId) { return Items.Remove(ItemId) > 0; }

	UFUNCTION(BlueprintPure, Category = "Ghost Hunter|Items")
	bool HasItem(FName ItemId) const { return Items.Contains(ItemId); }

	// ---- Hiding (almirah)

	UFUNCTION(BlueprintCallable, Category = "Ghost Hunter|Hiding")
	void EnterHiding(AHidingSpot* Spot);

	UFUNCTION(BlueprintCallable, Category = "Ghost Hunter|Hiding")
	void ExitHiding();

	UFUNCTION(BlueprintPure, Category = "Ghost Hunter|Hiding")
	bool IsHiding() const { return HidingSpot.IsValid(); }

	/** Seconds since the player got into the hiding spot (0 if not hiding). */
	UFUNCTION(BlueprintPure, Category = "Ghost Hunter|Hiding")
	float GetTimeHidden() const;

	/** The synthesised horror sound (heartbeat, ambience, whispers, stings). */
	UHorrorSynthWave* GetHorrorAudio() const { return HorrorWave; }

	/** Effects-only synth that keeps playing while the game is paused (menu / shop clicks). */
	UHorrorSynthWave* GetUiAudio() const { return UiWave; }

	/** Menu click, or the purchase jingle. For code that doesn't include HorrorSynthWave.h. */
	void PlayUiSound(bool bPurchase = false);

	/** Short message at the top of the screen. */
	UFUNCTION(BlueprintCallable, Category = "Ghost Hunter")
	void ShowMessage(const FText& Message, float Seconds = 3.f);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTorchComponent> Torch;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UGadgetComponent> Gadgets;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UViewModelComponent> ViewModel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float WalkSpeed = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float RunSpeed = 480.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float CrouchSpeed = 140.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	float LookSensitivity = 1.f;

	/** How far (cm) the player can reach to interact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float InteractDistance = 220.f;

	// Optional: set these in a Blueprint child to use editor-made input assets.
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> RunAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> CrouchAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> TorchAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> GuideAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> MenuAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ShopAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> MapAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> UseAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ControlsAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> RecordAction;

	/** Keys 1-5. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TArray<TObjectPtr<UInputAction>> GadgetSlotActions;

private:
	void BuildDefaultInput();
	void AddMappingContext();
	void RemoveMappingContext();

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void StartRun();
	void StopRun();
	void ToggleCrouch();
	void ToggleTorch();
	void Interact();
	void ToggleGuide();
	void ToggleMenu();
	void ToggleShop();
	void ToggleMap();
	void UseGadget();
	void ToggleControls();
	void ToggleRecording();
	void SelectSlot(int32 Slot);

	void UpdateFocus();
	void UpdateSpeed();

	TWeakObjectPtr<AActor> FocusedActor;
	bool bWantsToRun = false;

	UPROPERTY(VisibleInstanceOnly, Category = "Ghost Hunter|Items")
	TSet<FName> Items;

	void UpdateHorrorAudio(float DeltaSeconds);
	void UpdateFootsteps(float DeltaSeconds);

	/** Distance walked since the last footstep (cm). */
	float StepDistance = 0.f;

	TWeakObjectPtr<AHidingSpot> HidingSpot;
	float HideStartTime = 0.f;
	bool bTorchWasOn = false;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> HorrorAudio;

	UPROPERTY(Transient)
	TObjectPtr<UHorrorSynthWave> HorrorWave;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> UiAudio;

	UPROPERTY(Transient)
	TObjectPtr<UHorrorSynthWave> UiWave;
};
