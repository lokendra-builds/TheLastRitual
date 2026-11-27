#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gadgets.h"
#include "ViewModelComponent.generated.h"

class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class AGhostHunterCharacter;

/**
 * First-person "view model": the gadget Arjun holds, drawn bottom-right of the view.
 *
 * Models are built at BeginPlay and attached to the player camera, one per gadget.
 * Torch, UV Torch and Gun Torch use imported CC0 meshes under /Game/Gadgets (see
 * Scripts/import_gadget_assets.py); a gadget whose mesh is missing falls back to the
 * old engine-basic-shape placeholder. Thermometer and EMF meter are still built from
 * basic shapes. Only the equipped one is shown (none while hiding). Adds idle sway,
 * walk bob, look lag and a Gun Torch recoil kick (with the slide cycling).
 *
 * The models sit on lighting channel 1 only, lit by a small private fill light, so
 * the torch beam (which starts inside the model) doesn't blow them out and the fill
 * light never touches the room.
 */
UCLASS(ClassGroup = (GhostHunt), meta = (BlueprintSpawnableComponent))
class THELASTRITUAL_API UViewModelComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UViewModelComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Where the hand sits, relative to the camera (X forward, Y right, Z up, cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "View Model")
	FVector HoldOffset = FVector(34.f, 16.f, -13.5f);

	/** Base rotation of the held item relative to the camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "View Model")
	FRotator HoldRotation = FRotator(3.f, -3.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "View Model")
	float IdleSwayAmount = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "View Model")
	float WalkBobAmount = 0.9f;

	/** Candelas. Kept tiny: it only lights the view model (lighting channel 1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "View Model")
	float FillLightIntensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "View Model")
	float FillLightRadius = 70.f;

protected:
	virtual void BeginPlay() override;

private:
	AGhostHunterCharacter* GetHunter() const;

	void BuildModels(USceneComponent* Camera);
	USceneComponent* MakeGroup(const TCHAR* Name);
	UStaticMeshComponent* AddPart(USceneComponent* Parent, UStaticMesh* Mesh, const FVector& Loc, const FRotator& Rot,
		const FVector& SizeCm, const FLinearColor& Color, UMaterialInstanceDynamic** OutMat = nullptr);
	/** An imported mesh with its own materials, at real-world scale times Scale. */
	UStaticMeshComponent* AddMesh(USceneComponent* Parent, UStaticMesh* Mesh, const FVector& Loc, const FRotator& Rot, float Scale = 1.f);
	void SetupViewPart(UStaticMeshComponent* Part, USceneComponent* Parent);
	/** Basic-shape slab with rounded front corners, in a device frame (X depth, Y width, Z height). */
	void AddRoundedSlab(USceneComponent* Parent, const FTransform& Dev, const FVector& Center, float Depth, float Width,
		float Height, float Radius, const FLinearColor& Color);

	/** Gloved fist around a grip point (group-local). The held handle runs along the frame's +Z.
	 *  bThumbAlongHandle: thumb lies along the handle (torch) instead of across it (pistol). */
	void AddHand(USceneComponent* Parent, const FVector& GripLoc, const FRotator& GripRot, float Size = 0.85f, float HandleRadius = 0.f,
		bool bThumbAlongHandle = false);
	/** Jacket sleeve: a cylinder from the wrist running off-screen along Dir. */
	void AddSleeve(USceneComponent* Parent, const FVector& WristLoc, const FVector& Dir);
	void BuildTorch(USceneComponent* Group, const FLinearColor& HeadColor, const FLinearColor& LensColor);
	/** Imported torch mesh; returns false if it isn't there. */
	bool BuildRealTorch(USceneComponent* Group, UStaticMesh* Mesh, bool bUV);
	void BuildGunTorch(USceneComponent* Group);
	bool BuildRealGunTorch(USceneComponent* Group);
	void BuildThermometer(USceneComponent* Group);
	void BuildEMF(USceneComponent* Group);

	void UpdateVisibility(bool bHiding, EGadget Equipped);
	void UpdateMotion(float DeltaTime, AGhostHunterCharacter* Hunter, float Flash);
	void UpdateReadouts(EGadget Equipped);

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ShapeMaterial;

	/** Imported models (null if not imported; the placeholder is used instead). */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> TorchMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> UVTorchMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> WeaponLightMesh;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> PistolMeshes;

	/** Pistol slide, cycled back by the recoil. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> PistolSlide;
	FVector PistolSlideRest = FVector::ZeroVector;

	/** Moves with sway/bob/recoil; every model hangs off this. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> ViewRoot;

	/** One group per EGadget, in enum order. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> Models;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> FillLight;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> EMFLeds;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ThermoScreen;

	int32 ShownIndex = -2;
	int32 LastEMFLevel = -1;
	int32 LastThermoCold = -1;

	float LastFlash = 0.f;
	float Recoil = 0.f;
	float RecoilVel = 0.f;
	float SwayTime = 0.f;
	float BobPhase = 0.f;
	float BobWeight = 0.f;
	FRotator LastControlRot = FRotator::ZeroRotator;
	FRotator LookLag = FRotator::ZeroRotator;
	bool bHaveControlRot = false;
};
