#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GhostHuntGameMode.generated.h"

/**
 * Default game mode: spawns the ghost hunter and the HUD.
 *
 * Testing aid: launch with -AutoShot=<seconds> [-AutoShotYaw=<deg>] [-AutoShotPitch=<deg>]
 * to take a screenshot after that many seconds and quit. Used to tune lighting
 * without someone at the keyboard.
 */
UCLASS()
class THELASTRITUAL_API AGhostHuntGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGhostHuntGameMode();

protected:
	virtual void BeginPlay() override;

private:
	void TakeAutoShot();
	void QuitAfterShot();

	FTimerHandle AutoShotTimer;
};
