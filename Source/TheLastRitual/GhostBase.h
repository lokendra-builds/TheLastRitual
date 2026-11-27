#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/TimerHandle.h"
#include "GhostBase.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInterface;
class UStaticMesh;
class AGhostHunterCharacter;

/**
 * The ghost's body. Mostly hidden; the Horror Director and wrong rituals
 * make it appear. Scripted, not a thinking AI (by design).
 *
 * Body: the Mirror Woman built from engine basic shapes (pale sari, long black
 * hair over the face, thin arms, faint red eyes). Make a Blueprint child to swap
 * in a real model and hook sounds/animation to OnAppeared / OnJumpScare.
 */
UCLASS()
class THELASTRITUAL_API AGhostBase : public AActor
{
	GENERATED_BODY()

public:
	AGhostBase();

	/** Show the ghost at a spot, facing the player, then vanish after Duration. */
	UFUNCTION(BlueprintCallable, Category = "Ghost")
	void Appear(FVector FeetLocation, float Duration);

	UFUNCTION(BlueprintCallable, Category = "Ghost")
	void AppearAtRandomPoint(float Duration);

	/** Right in the player's face, then the torch dies. */
	UFUNCTION(BlueprintCallable, Category = "Ghost")
	void JumpScare(AGhostHunterCharacter* Hunter);

	UFUNCTION(BlueprintCallable, Category = "Ghost")
	void Vanish();

	// ---- Hunt: the ghost comes for the player.
	//
	// It spawns at a far appear point and walks the nav mesh towards the player.
	// While it can see the player it follows them; once it loses sight it goes to
	// where it last saw them and gives up if they're gone (closing a door blocks
	// its sight). Touching the player = caught: lose a life + jump scare.

	UFUNCTION(BlueprintCallable, Category = "Ghost|Hunt")
	void StartHunt(float Duration);

	UFUNCTION(BlueprintCallable, Category = "Ghost|Hunt")
	void EndHunt(bool bCaughtPlayer = false);

	UFUNCTION(BlueprintPure, Category = "Ghost|Hunt")
	bool IsHunting() const { return bHunting; }

	UFUNCTION(BlueprintPure, Category = "Ghost|Hunt")
	bool IsBanished() const { return bBanished; }

	/** True during the few seconds of warning before she shows up. */
	UFUNCTION(BlueprintPure, Category = "Ghost|Hunt")
	bool IsHuntWarning() const { return bHunting && bHuntWarning; }

	/** 0 at the start of the warning, 1 when she appears. */
	UFUNCTION(BlueprintPure, Category = "Ghost|Hunt")
	float GetHuntWarningProgress() const { return HuntWarningTime > 0.f ? 1.f - FMath::Clamp(WarningLeft / HuntWarningTime, 0.f, 1.f) : 1.f; }

	// ---- Fighting back (Gun Torch)

	/** A hit from the Gun Torch. She staggers; at 0 health she is driven off for this hunt. */
	UFUNCTION(BlueprintCallable, Category = "Ghost|Combat")
	void TakeHit(float Damage);

	UFUNCTION(BlueprintPure, Category = "Ghost|Combat")
	float GetHealthPercent() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }

	/** Visible and chasing: can be shot. */
	UFUNCTION(BlueprintPure, Category = "Ghost|Combat")
	bool CanBeHit() const { return bHunting && !bHuntWarning && !IsHidden(); }

	/** Where she's coming from during a hunt: her spawn point during the warning, then her body. */
	FVector GetThreatLocation() const { return bHuntWarning ? HuntSpawnLocation : GetActorLocation(); }

	/** Where she lingers when not hunting (cold spots, EMF). Set from where she's placed. */
	FVector GetHomeLocation() const { return HomeLocation; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Combat")
	float MaxHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Combat")
	int32 PointsForDrivingOff = 2000;

	UFUNCTION(BlueprintImplementableEvent, Category = "Ghost|Combat")
	void OnDrivenOff();

	/** Seconds of warning (torch flicker, silence, heartbeat) before she appears. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Hunt")
	float HuntWarningTime = 6.f;

	/** She starts slow and reaches full HuntSpeed after this many seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Hunt")
	float HuntRampTime = 5.f;

	/** Walk speed 250, run 480: you can outrun her, but not by much. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Hunt")
	float HuntSpeed = 330.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Hunt")
	float CatchDistance = 90.f;

	/** Hunts start at an appear point at least this far (cm) from the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Hunt")
	float MinSpawnDistance = 1800.f;

	UFUNCTION(BlueprintImplementableEvent, Category = "Ghost|Hunt")
	void OnHuntStarted();

	UFUNCTION(BlueprintImplementableEvent, Category = "Ghost|Hunt")
	void OnHuntEnded(bool bCaughtPlayer);

	/** Places the ghost may show itself (use Target Point actors in corridors, doorways). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost")
	TArray<TObjectPtr<AActor>> AppearPoints;

	UFUNCTION(BlueprintImplementableEvent, Category = "Ghost")
	void OnAppeared();

	UFUNCTION(BlueprintImplementableEvent, Category = "Ghost")
	void OnJumpScare();

	UFUNCTION(BlueprintImplementableEvent, Category = "Ghost")
	void OnBanished();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> EyeL;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> EyeR;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> EyeGlow;

	/** Everything visible hangs off this, so it can sway/float without moving the actor (origin stays at her feet). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Figure;

	/** The hair over her face, pivoting at the crown. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> HairFront;

	/** M_Ghost (Color / Glow / Rim parameters); falls back to the engine shape material. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Look")
	TObjectPtr<UMaterialInterface> GhostMaterial;

private:
	UFUNCTION()
	void HandleRitual(FName RitualId, bool bCorrect);

	void FacePlayer();
	void TickHunt(float DeltaSeconds);
	bool CanSeePlayer(const AGhostHunterCharacter* Hunter) const;
	void RepathTo(const FVector& Target);

	struct FPartLook
	{
		FLinearColor Color;
		float Glow = 0.f;
		float Rim = 0.f;
	};
	UStaticMeshComponent* AddPart(const TCHAR* Name, UStaticMesh* Mesh, USceneComponent* Parent, const FVector& Loc,
		const FVector& SizeCm, const FRotator& Rot, const FLinearColor& Color, float Glow, float Rim, bool bShadow = true);
	void TickLook(float DeltaSeconds);

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;
	TArray<FPartLook> PartLooks;
	float SwayTime = 0.f;

	bool bBanished = false;
	FTimerHandle VanishTimer;

	float Health = 100.f;
	float StaggerLeft = 0.f;
	FVector HomeLocation = FVector::ZeroVector;

	bool bHunting = false;
	bool bHuntWarning = false;
	float WarningLeft = 0.f;
	float ChaseTime = 0.f;
	bool bSawPlayerHide = false;
	bool bSeesPrev = false;
	FVector HuntSpawnLocation = FVector::ZeroVector;
	float HuntTimeLeft = 0.f;
	float RepathTimer = 0.f;
	float FlickerTimer = 0.f;
	bool bHasLastSeen = false;
	FVector LastSeenLocation = FVector::ZeroVector;
	TArray<FVector> PathPoints;
	int32 PathIndex = 0;
};
