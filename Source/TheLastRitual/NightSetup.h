#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NightSetup.generated.h"

class UGhostDefinition;

/**
 * Put one of these in each night's level and pick tonight's ghost.
 * Starts the night when play begins.
 */
UCLASS()
class THELASTRITUAL_API ANightSetup : public AActor
{
	GENERATED_BODY()

public:
	ANightSetup();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night")
	TObjectPtr<UGhostDefinition> TonightGhost;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night", meta = (ClampMin = 1))
	int32 StartingLives = 3;

	/** Horror pacing. Calm becomes Restless after this many seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Horror Director")
	float RestlessAfterSeconds = 180.f;

	/** Min/max seconds between scare events, per anger level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Horror Director")
	FVector2D CalmEventInterval = FVector2D(40.f, 80.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Horror Director")
	FVector2D RestlessEventInterval = FVector2D(20.f, 40.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Horror Director")
	FVector2D AngryEventInterval = FVector2D(10.f, 22.f);

	/** No sudden ghost hunts before this many seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Horror Director|Hunt")
	float FirstHuntAfterSeconds = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Horror Director|Hunt")
	float MinSecondsBetweenHunts = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Horror Director|Hunt")
	float HuntDuration = 25.f;

	/** Chance a scare becomes a hunt: Calm, Restless, Angry, Furious. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Horror Director|Hunt")
	FVector4 HuntChance = FVector4(0.1f, 0.3f, 0.45f, 0.6f);

protected:
	virtual void BeginPlay() override;
};
