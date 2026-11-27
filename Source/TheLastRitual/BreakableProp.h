#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BreakableProp.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

UENUM(BlueprintType)
enum class EBreakableKind : uint8
{
	ClayPot,   // terracotta shards, a puff of dust
	Crate,     // wood splinters
	Lantern,   // glass + brass bits, its light goes out
	Glass      // bottle / jar
};

/**
 * Something a Gun Torch shot can smash. Breaks into physics shards that
 * settle and fade, plays a sound, and (lanterns) puts out its light.
 *
 * Give it any mesh in the editor or from a script (Mesh component); the
 * shards take that mesh's colour/material.
 */
UCLASS()
class THELASTRITUAL_API ABreakableProp : public AActor
{
	GENERATED_BODY()

public:
	ABreakableProp();

	UFUNCTION(BlueprintCallable, Category = "Breakable")
	void Break(const FVector& HitPoint, const FVector& ShotDirection);

	UFUNCTION(BlueprintPure, Category = "Breakable")
	bool IsBroken() const { return bBroken; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable")
	EBreakableKind Kind = EBreakableKind::ClayPot;

	/** Number of shards it breaks into. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable", meta = (ClampMin = 2, ClampMax = 40))
	int32 ShardCount = 12;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Optional light (lanterns): switched off when broken. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable")
	bool bHasLight = false;

protected:
	virtual void BeginPlay() override;

private:
	bool bBroken = false;
};
