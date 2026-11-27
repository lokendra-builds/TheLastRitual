#include "RoomArea.h"

#include "Components/BoxComponent.h"

ARoomArea::ARoomArea()
{
	Area = CreateDefaultSubobject<UBoxComponent>(TEXT("Area"));
	RootComponent = Area;
	Area->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Area->SetCanEverAffectNavigation(false);
	Area->SetBoxExtent(FVector(200.f, 200.f, 180.f));
	Area->ShapeColor = FColor(255, 170, 60);
}

bool ARoomArea::ContainsPoint(const FVector& WorldPoint) const
{
	const FVector Local = Area->GetComponentTransform().InverseTransformPosition(WorldPoint);
	const FVector E = Area->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= E.X && FMath::Abs(Local.Y) <= E.Y && FMath::Abs(Local.Z) <= E.Z;
}

FBox2D ARoomArea::GetFloorRect() const
{
	const FBox B = Area->Bounds.GetBox();
	return FBox2D(FVector2D(B.Min.X, B.Min.Y), FVector2D(B.Max.X, B.Max.Y));
}
