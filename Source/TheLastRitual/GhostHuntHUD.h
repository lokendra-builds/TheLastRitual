#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GhostGuideSubsystem.h"
#include "GhostHuntHUD.generated.h"

/**
 * First-draft HUD drawn with Canvas (no widget assets needed):
 * dot crosshair, torch battery bar, interaction prompt,
 * "Added to Ghost Guide" message, and the Ghost Guide page (Tab).
 * Gets replaced by proper UMG widgets once the look is decided.
 */
UCLASS()
class THELASTRITUAL_API AGhostHuntHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ToggleGhostGuide() { bGuideOpen = !bGuideOpen; }

	UFUNCTION(BlueprintPure, Category = "HUD")
	bool IsGhostGuideOpen() const { return bGuideOpen; }

	/** Esc menu: pauses the game, shows the mouse, Resume / Restart night / Exit game. */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ToggleMenu();

	UFUNCTION(BlueprintPure, Category = "HUD")
	bool IsMenuOpen() const { return bMenuOpen; }

	/** B: the gadget shop. Pauses the game and shows the mouse. */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ToggleShop();

	UFUNCTION(BlueprintPure, Category = "HUD")
	bool IsShopOpen() const { return bShopOpen; }

	/** H: show/hide the controls panel. */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ToggleControls() { bControlsOpen = !bControlsOpen; }

	/** M: the haveli map. */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ToggleMap() { bMapOpen = !bMapOpen; }

	virtual void NotifyHitBoxClick(FName BoxName) override;
	virtual void NotifyHitBoxBeginCursorOver(FName BoxName) override;
	virtual void NotifyHitBoxEndCursorOver(FName BoxName) override;

	/** Short message at the top of the screen for a few seconds. */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ShowMessage(const FText& Message, float Seconds = 3.f);

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleClueRecorded(const FFoundClue& Clue);

	UFUNCTION()
	void HandleRitualPerformed(FName RitualId, bool bCorrect);

	void DrawGameplay(class UFont* Font);
	void DrawGhostGuide(class UFont* Font);
	void DrawLives(class UFont* Font);
	void DrawHuntVignette();
	void DrawHidingSlats(class UFont* Font);
	void DrawObjectives(class UFont* Font);
	void DrawMenu(class UFont* Font);
	void DrawButton(class UFont* Font, FName Id, const FString& Label, float X, float Y, float W, float H);
	void SetMenuOpen(bool bOpen);
	void SetShopOpen(bool bOpen);
	void ApplyCursorMode();
	void DrawTopBar(class UFont* Font);
	void DrawHotbar(class UFont* Font);
	void DrawGadgetReadout(class UFont* Font);
	void DrawGhostHealth(class UFont* Font);
	void DrawCompass(class UFont* Font);
	void DrawWaypoint(class UFont* Font);
	void DrawFloorPath();          // GhostHuntHUD_Path.cpp
	void UpdateFloorPath();        // GhostHuntHUD_Path.cpp
	void DrawControlsPanel(class UFont* Font);
	void DrawMap(class UFont* Font);
	void UpdateNavTargets();
	const class ARoomArea* RoomAt(const FVector& Point) const;
	void DrawShop(class UFont* Font);
	void DrawGadgetIcon(uint8 Gadget, float X, float Y, float Size, const FLinearColor& Color);
	void DrawNightResult(class UFont* Font);

	bool bGuideOpen = false;
	bool bMenuOpen = false;
	bool bShopOpen = false;
	bool bMapOpen = false;
	bool bControlsOpen = true;

	/** Realistic gadget icons (/Game/UI/Icons/T_Icon_<Gadget>); drawn pictograms are the fallback. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UTexture2D>> GadgetIcons;

	// Compass / map: refreshed a few times a second.
	float NavRefreshTimer = 0.f;
	TWeakObjectPtr<const class ARoomArea> CurrentRoom;
	TWeakObjectPtr<const class ARoomArea> ObjectiveRoom;
	bool bHasObjective = false;
	bool bObjectiveInRoom = false;      // true: pointing at a sign in the room you're in
	FVector ObjectiveLocation = FVector::ZeroVector;
	FString ObjectiveLabel;
	void DrawGhostDirection(class UFont* Font);

	// Path on the floor to the objective room (GhostHuntHUD_Path.cpp).
	TArray<FVector> FloorPath;     // resampled points, every ~70 cm
	float FloorPathTimer = 0.f;
	FName HoveredButton;
	FText MessageText;
	float MessageHideTime = 0.f;
};
