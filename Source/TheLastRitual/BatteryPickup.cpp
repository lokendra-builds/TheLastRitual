#include "BatteryPickup.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "GhostHunterCharacter.h"
#include "TorchComponent.h"
#include "HorrorSynthWave.h"

#define LOCTEXT_NAMESPACE "GhostHunt"

ABatteryPickup::ABatteryPickup()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;

	// Placeholder: small cylinder, roughly battery-sized (3 x 3 x 6 cm).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		Mesh->SetStaticMesh(Cylinder.Object);
		Mesh->SetRelativeScale3D(FVector(0.03f, 0.03f, 0.06f));
	}
}

void ABatteryPickup::Interact_Implementation(AGhostHunterCharacter* Hunter)
{
	if (Hunter && Hunter->GetTorch())
	{
		Hunter->GetTorch()->AddBattery(Amount);
		UHorrorSynthWave::Play(this, EHorrorSfx::BatteryPickup);
		Destroy();
	}
}

FText ABatteryPickup::GetInteractPrompt_Implementation() const
{
	return LOCTEXT("PickUpBattery", "Pick up battery");
}

#undef LOCTEXT_NAMESPACE
