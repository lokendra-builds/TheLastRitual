#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RoomArea.generated.h"

class UBoxComponent;

/**
 * Names a part of the house ("Kitchen", "Courtyard"...). Used by the HUD for
 * "you are here", the compass objective marker, and the map (M).
 * The box is the room's floor area; it never blocks anything.
 */
UCLASS()
class THELASTRITUAL_API ARoomArea : public AActor
{
	GENERATED_BODY()

public:
	ARoomArea();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	FText RoomName;

	/** Draw this on the map (corridors and the outside may want false). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	bool bShowOnMap = true;

	UFUNCTION(BlueprintPure, Category = "Room")
	bool ContainsPoint(const FVector& WorldPoint) const;

	/** Floor rectangle in world space (min/max X,Y). */
	FBox2D GetFloorRect() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> Area;
};
