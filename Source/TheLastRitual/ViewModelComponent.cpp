#include "ViewModelComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Controller.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

#include "GadgetComponent.h"
#include "GhostHunterCharacter.h"

namespace ViewModelAssets
{
	// Imported by Scripts/import_gadget_assets.py (CC0 Poly Haven models, see context/asset-licences.md).
	const TCHAR* Torch = TEXT("/Game/Gadgets/Torch/SM_Torch/StaticMeshes/SM_Torch.SM_Torch");
	const TCHAR* UVTorch = TEXT("/Game/Gadgets/UVTorch/SM_UVTorch/StaticMeshes/SM_UVTorch.SM_UVTorch");
	const TCHAR* WeaponLight = TEXT("/Game/Gadgets/WeaponLight/SM_WeaponLight/StaticMeshes/SM_WeaponLight.SM_WeaponLight");
	// The pistol comes in as four parts sharing one origin: frame (with barrel), slide, hammer, trigger.
	const TCHAR* Pistol[] = {
		TEXT("/Game/Gadgets/GunTorch/SM_Pistol/StaticMeshes/service_pistol_pistol_a.service_pistol_pistol_a"),
		TEXT("/Game/Gadgets/GunTorch/SM_Pistol/StaticMeshes/service_pistol_slide_a.service_pistol_slide_a"),
		TEXT("/Game/Gadgets/GunTorch/SM_Pistol/StaticMeshes/service_pistol_hammer_a.service_pistol_hammer_a"),
		TEXT("/Game/Gadgets/GunTorch/SM_Pistol/StaticMeshes/service_pistol_trigger_a.service_pistol_trigger_a"),
	};
	constexpr int32 PistolSlideIndex = 1;

	UStaticMesh* Load(const TCHAR* Path)
	{
		return LoadObject<UStaticMesh>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	/** The torch mesh lies along Y with its head at +Y and the lanyard loop on +X.
	 *  Turn it to point forward (+X) with the lanyard hanging down, or tucked to the right (hidden side). */
	FRotator TorchForward(bool bLanyardRight = false)
	{
		return bLanyardRight
			? FMatrix(FVector(0.f, 1.f, 0.f), FVector(1.f, 0.f, 0.f), FVector(0.f, 0.f, -1.f), FVector::ZeroVector).Rotator()
			: FMatrix(FVector(0.f, 0.f, -1.f), FVector(1.f, 0.f, 0.f), FVector(0.f, -1.f, 0.f), FVector::ZeroVector).Rotator();
	}
}

namespace
{
	// Hold tuning for the imported models (group-local, cm / degrees).
	const float TorchX = 2.f;
	const FVector TorchHoldLoc(10.f, 3.f, -4.5f);
	const FRotator TorchHoldRot(2.f, -7.f, 0.f);
	const FVector GunLoc(8.f, 3.f, -4.5f);
	const FRotator GunRot(0.f, -16.f, -6.f);
	const FVector LightLoc(12.6f, 0.f, 2.9f);
	const float LightScale = 0.4f;
	const FVector HandLoc(-0.7f, 0.f, 0.5f);
	const FRotator HandRot(-18.f, 0.f, 0.f);
	const FVector SleeveLoc(-6.f, 1.5f, -7.5f);
	const FVector SleeveDir(-0.75f, 0.35f, -0.6f);
	// Thermometer / EMF meter (basic-shape models) sit a little lower and further out than the hold point.
	const FVector DeviceOffset(4.f, 2.f, -2.5f);
}

namespace ViewModelColors
{
	const FLinearColor Skin(0.30f, 0.18f, 0.12f);
	const FLinearColor SkinDark(0.24f, 0.14f, 0.09f);
	const FLinearColor Glove(0.014f, 0.012f, 0.011f);     // worn black leather
	const FLinearColor GloveDark(0.008f, 0.007f, 0.007f);
	const FLinearColor Sleeve(0.05f, 0.055f, 0.07f);
	const FLinearColor Cuff(0.03f, 0.032f, 0.04f);
	const FLinearColor WeaponLight(0.045f, 0.045f, 0.05f); // anodised black tint for the gun light
	const FLinearColor Metal(0.035f, 0.035f, 0.04f);
	const FLinearColor MetalLight(0.12f, 0.12f, 0.13f);
	const FLinearColor Rubber(0.015f, 0.015f, 0.015f);
	const FLinearColor Plastic(0.18f, 0.16f, 0.12f);
	const FLinearColor Lens(1.f, 0.9f, 0.6f);
	const FLinearColor UVHead(0.22f, 0.06f, 0.5f);
	const FLinearColor UVLens(0.6f, 0.25f, 1.f);
	const FLinearColor Brass(0.4f, 0.22f, 0.06f);
	const FLinearColor ScreenWarm(0.22f, 0.34f, 0.24f);   // backlit LCD
	const FLinearColor ScreenCold(0.1f, 0.25f, 0.6f);
	const FLinearColor Holster(0.55f, 0.32f, 0.02f);   // yellow rubber boot
	const FLinearColor Face(0.035f, 0.037f, 0.04f);
	const FLinearColor ButtonGrey(0.09f, 0.09f, 0.095f);
	const FLinearColor EMFBody(0.014f, 0.015f, 0.016f);
	const FLinearColor Label(0.16f, 0.16f, 0.15f);
	const FLinearColor LabelInk(0.02f, 0.02f, 0.02f);
}

UViewModelComponent::UViewModelComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	CubeMesh = Cube.Object;
	CylinderMesh = Cylinder.Object;
	SphereMesh = Sphere.Object;
	// The Cube's own default is the world-grid material, which has no "Color" parameter.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BaseMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	ShapeMaterial = BaseMat.Object;
}

AGhostHunterCharacter* UViewModelComponent::GetHunter() const
{
	return Cast<AGhostHunterCharacter>(GetOwner());
}

void UViewModelComponent::BeginPlay()
{
	Super::BeginPlay();

	AGhostHunterCharacter* Hunter = GetHunter();
	if (!Hunter || !Hunter->GetCamera() || !CubeMesh || !CylinderMesh)
	{
		SetComponentTickEnabled(false);
		return;
	}
	// Only the local player needs a view model.
	if (!Hunter->IsLocallyControlled() && Hunter->GetController() != nullptr)
	{
		SetComponentTickEnabled(false);
		return;
	}
	TorchMesh = ViewModelAssets::Load(ViewModelAssets::Torch);
	UVTorchMesh = ViewModelAssets::Load(ViewModelAssets::UVTorch);
	WeaponLightMesh = ViewModelAssets::Load(ViewModelAssets::WeaponLight);
	PistolMeshes.Reset();
	for (const TCHAR* Path : ViewModelAssets::Pistol)
	{
		PistolMeshes.Add(ViewModelAssets::Load(Path));
	}
	BuildModels(Hunter->GetCamera());
}

// ---------------------------------------------------------------- Building

USceneComponent* UViewModelComponent::MakeGroup(const TCHAR* Name)
{
	USceneComponent* Group = NewObject<USceneComponent>(GetOwner(), Name);
	Group->SetupAttachment(ViewRoot);
	Group->RegisterComponent();
	return Group;
}

UStaticMeshComponent* UViewModelComponent::AddPart(USceneComponent* Parent, UStaticMesh* Mesh, const FVector& Loc,
	const FRotator& Rot, const FVector& SizeCm, const FLinearColor& Color, UMaterialInstanceDynamic** OutMat)
{
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(GetOwner());
	Part->SetStaticMesh(Mesh);
	if (ShapeMaterial)
	{
		Part->SetMaterial(0, ShapeMaterial);
	}
	SetupViewPart(Part, Parent);
	// Basic shapes are 100 cm; cylinders are 100 cm across and 100 cm tall (along Z).
	Part->SetRelativeLocationAndRotation(Loc, Rot);
	Part->SetRelativeScale3D(SizeCm / 100.f);
	Part->RegisterComponent();

	if (UMaterialInstanceDynamic* Mat = Part->CreateDynamicMaterialInstance(0))
	{
		Mat->SetVectorParameterValue(TEXT("Color"), Color);
		if (OutMat)
		{
			*OutMat = Mat;
		}
	}
	return Part;
}

UStaticMeshComponent* UViewModelComponent::AddMesh(USceneComponent* Parent, UStaticMesh* Mesh, const FVector& Loc, const FRotator& Rot, float Scale)
{
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(GetOwner());
	Part->SetStaticMesh(Mesh);
	SetupViewPart(Part, Parent);
	Part->SetRelativeLocationAndRotation(Loc, Rot);
	Part->SetRelativeScale3D(FVector(Scale));
	Part->RegisterComponent();
	return Part;
}

void UViewModelComponent::SetupViewPart(UStaticMeshComponent* Part, USceneComponent* Parent)
{
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetCollisionResponseToAllChannels(ECR_Ignore);
	Part->SetGenerateOverlapEvents(false);
	Part->SetCanEverAffectNavigation(false);
	Part->CastShadow = false;
	Part->bCastDynamicShadow = false;
	Part->bCastStaticShadow = false;
	Part->bReceivesDecals = false;
	Part->bAffectDistanceFieldLighting = false;
	Part->bAffectDynamicIndirectLighting = false;
	Part->SetVisibleInRayTracing(false);
	// Lit only by our own fill light (channel 1), not the torch beam that starts inside the model.
	Part->LightingChannels.bChannel0 = false;
	Part->LightingChannels.bChannel1 = true;
	Part->SetupAttachment(Parent);
}

void UViewModelComponent::AddHand(USceneComponent* Parent, const FVector& GripLoc, const FRotator& GripRot, float Size, float HandleRadius,
	bool bThumbAlongHandle)
{
	// Hand frame: the held handle runs along +Z, fingers wrap round the front (+X) and come
	// back along the left side (-Y), palm on the right (+Y), thumb over the top-left.
	// Fingers are capsules (cylinder + sphere ends) so it reads as a gloved hand, not boxes.
	// Designed round a ~3 cm handle; a thicker handle (HandleRadius, cm) pushes every part outwards.
	const FTransform Frame(GripRot, GripLoc);
	const float K = Size;
	const float Grow = HandleRadius > 0.f ? FMath::Max(0.f, HandleRadius - 1.5f * K) : 0.f;
	UStaticMesh* Round = SphereMesh ? SphereMesh.Get() : CubeMesh.Get();
	auto ToGroup = [&](const FVector& L)
	{
		const FVector Push(L.X > 1.f ? Grow : (L.X < -1.f ? -Grow : 0.f), L.Y > 1.f ? Grow : (L.Y < -1.f ? -Grow : 0.f), 0.f);
		return Frame.TransformPosition(L * K + Push);
	};
	auto Blob = [&](const FVector& L, const FVector& S, const FLinearColor& C)
	{
		AddPart(Parent, Round, ToGroup(L), GripRot, S * K, C);
	};
	auto Capsule = [&](const FVector& A, const FVector& B, float Diam, const FLinearColor& C)
	{
		const FVector PA = ToGroup(A), PB = ToGroup(B);
		const FVector D = PB - PA;
		const float Len = D.Size();
		const float Dm = Diam * K;
		if (Len > KINDA_SMALL_NUMBER)
		{
			AddPart(Parent, CylinderMesh, (PA + PB) * 0.5f, FRotationMatrix::MakeFromZ(D).Rotator(), FVector(Dm, Dm * 0.92f, Len), C);
		}
		AddPart(Parent, Round, PA, GripRot, FVector(Dm), C);
		AddPart(Parent, Round, PB, GripRot, FVector(Dm * 0.96f), C);
	};

	Blob(FVector(-0.3f, 2.5f, -0.4f), FVector(5.0f, 2.4f, 8.2f), ViewModelColors::GloveDark);   // palm / back of hand
	Blob(FVector(-0.7f, 2.3f, -4.5f), FVector(4.4f, 2.8f, 3.4f), ViewModelColors::GloveDark);   // heel of the hand
	Capsule(FVector(1.2f, 2.1f, 2.5f), FVector(1.2f, 2.1f, -2.6f), 2.1f, ViewModelColors::Glove); // knuckle ridge
	for (int32 i = 0; i < 4; ++i)
	{
		const float Z = 2.55f - i * 1.85f;
		const float L = (i == 3) ? 0.78f : (i == 0 ? 0.95f : 1.f); // little finger shorter
		const float D = (i == 3) ? 1.55f : 1.8f;
		const FVector Knuckle(1.2f, 2.0f, Z);
		const FVector Mid(2.35f, -0.5f * L, Z);
		const FVector Tip(1.0f, -2.2f, Z - 0.1f);
		const FVector End(-0.3f * L, -2.0f, Z - 0.15f);
		Capsule(Knuckle, Mid, D, ViewModelColors::Glove);
		Capsule(Mid, Tip, D * 0.95f, ViewModelColors::Glove);
		Capsule(Tip, End, D * 0.88f, ViewModelColors::GloveDark);
	}
	if (bThumbAlongHandle)
	{
		// Thumb laid along the top of the handle, pointing at the switch.
		Capsule(FVector(-1.4f, 1.4f, 1.2f), FVector(-2.0f, -0.2f, 3.6f), 1.85f, ViewModelColors::Glove);
		Capsule(FVector(-2.0f, -0.2f, 3.6f), FVector(-2.1f, -0.5f, 5.8f), 1.6f, ViewModelColors::Glove);
	}
	else
	{
		// Thumb across the top-left of the grip, pointing forward under the slide.
		Capsule(FVector(-2.1f, 0.6f, 3.6f), FVector(-0.8f, -1.9f, 4.0f), 1.85f, ViewModelColors::Glove);
		Capsule(FVector(-0.8f, -1.9f, 4.0f), FVector(1.2f, -2.1f, 4.2f), 1.6f, ViewModelColors::Glove);
	}
}

void UViewModelComponent::AddSleeve(USceneComponent* Parent, const FVector& WristLoc, const FVector& Dir)
{
	const FVector D = Dir.GetSafeNormal();
	const FRotator Rot = FRotationMatrix::MakeFromZ(D).Rotator();
	AddPart(Parent, CylinderMesh, WristLoc - D * 1.5f, Rot, FVector(5.0f, 4.4f, 5.0f), ViewModelColors::Glove); // gloved wrist
	AddPart(Parent, CylinderMesh, WristLoc + D * 1.2f, Rot, FVector(6.4f, 6.4f, 3.0f), ViewModelColors::Cuff);  // knit cuff
	AddPart(Parent, CylinderMesh, WristLoc + D * 11.f, Rot, FVector(8.4f, 8.0f, 17.f), ViewModelColors::Sleeve); // jacket sleeve
	AddPart(Parent, CylinderMesh, WristLoc + D * 26.f, Rot, FVector(9.6f, 9.0f, 14.f), ViewModelColors::Sleeve);
}

void UViewModelComponent::BuildTorch(USceneComponent* Group, const FLinearColor& HeadColor, const FLinearColor& LensColor)
{
	const FRotator Fwd(90.f, 0.f, 0.f); // cylinder axis -> forward
	AddPart(Group, CylinderMesh, FVector(-5.8f, 0.f, 0.f), Fwd, FVector(3.4f, 3.4f, 1.2f), ViewModelColors::Rubber); // tail cap
	AddPart(Group, CylinderMesh, FVector(2.5f, 0.f, 0.f), Fwd, FVector(3.2f, 3.2f, 16.f), ViewModelColors::Metal);  // handle
	AddPart(Group, CylinderMesh, FVector(11.2f, 0.f, 0.f), Fwd, FVector(4.2f, 4.2f, 2.4f), ViewModelColors::Metal); // neck
	AddPart(Group, CylinderMesh, FVector(14.6f, 0.f, 0.f), Fwd, FVector(5.6f, 5.6f, 5.f), HeadColor);              // head
	AddPart(Group, CylinderMesh, FVector(17.2f, 0.f, 0.f), Fwd, FVector(4.8f, 4.8f, 0.4f), LensColor);             // lens
	AddPart(Group, CubeMesh, FVector(6.5f, 0.f, 1.7f), FRotator::ZeroRotator, FVector(2.f, 1.2f, 0.8f), ViewModelColors::Rubber); // switch

	// Handle runs forward: hand frame Z -> forward, fingers (frame +X) underneath.
	AddHand(Group, FVector(0.5f, 0.f, 0.f), FRotator(-90.f, 0.f, 0.f));
	{ const FRotator SleeveRot(18.f, -18.f, 0.f); AddSleeve(Group, FVector(-10.f, 3.5f, -4.f) + SleeveRot.Vector() * 6.f, -SleeveRot.Vector()); }
}

void UViewModelComponent::BuildGunTorch(USceneComponent* Group)
{
	const FRotator Fwd(90.f, 0.f, 0.f);
	const FRotator GripRot(-15.f, 0.f, 0.f);
	AddPart(Group, CubeMesh, FVector(5.f, 0.f, 4.4f), FRotator::ZeroRotator, FVector(17.f, 2.8f, 3.2f), ViewModelColors::Metal);        // slide
	AddPart(Group, CubeMesh, FVector(5.f, 0.f, 6.05f), FRotator::ZeroRotator, FVector(15.f, 1.0f, 0.3f), ViewModelColors::MetalLight);   // rib
	AddPart(Group, CubeMesh, FVector(-3.f, 0.f, 6.3f), FRotator::ZeroRotator, FVector(0.8f, 1.6f, 0.8f), ViewModelColors::MetalLight);  // rear sight
	AddPart(Group, CylinderMesh, FVector(14.2f, 0.f, 4.6f), Fwd, FVector(1.5f, 1.5f, 1.6f), ViewModelColors::Rubber);                   // muzzle
	AddPart(Group, CubeMesh, FVector(4.5f, 0.f, 2.1f), FRotator::ZeroRotator, FVector(12.f, 2.5f, 1.6f), ViewModelColors::MetalLight);  // frame
	AddPart(Group, CubeMesh, FVector(-1.f, 0.f, -1.6f), GripRot, FVector(3.6f, 2.7f, 9.f), ViewModelColors::Rubber);                    // grip
	AddPart(Group, CubeMesh, FVector(3.6f, 0.f, -0.4f), FRotator::ZeroRotator, FVector(4.f, 0.7f, 0.6f), ViewModelColors::Metal);       // trigger guard
	AddPart(Group, CubeMesh, FVector(5.4f, 0.f, 0.5f), FRotator::ZeroRotator, FVector(0.6f, 0.7f, 2.2f), ViewModelColors::Metal);
	// Torch slung under the barrel.
	AddPart(Group, CubeMesh, FVector(9.5f, 0.f, 0.9f), FRotator::ZeroRotator, FVector(5.f, 1.6f, 1.4f), ViewModelColors::Metal);        // rail mount
	AddPart(Group, CylinderMesh, FVector(10.5f, 0.f, -0.6f), Fwd, FVector(2.8f, 2.8f, 6.f), ViewModelColors::Metal);
	AddPart(Group, CylinderMesh, FVector(14.2f, 0.f, -0.6f), Fwd, FVector(3.6f, 3.6f, 2.f), ViewModelColors::Brass);
	AddPart(Group, CylinderMesh, FVector(15.3f, 0.f, -0.6f), Fwd, FVector(3.0f, 3.0f, 0.3f), ViewModelColors::Lens);

	AddHand(Group, FVector(-1.f, 0.f, -2.2f), GripRot);
	{ const FRotator SleeveRot(48.f, -15.f, 0.f); AddSleeve(Group, FVector(-6.5f, 4.f, -11.f) + SleeveRot.Vector() * 6.f, -SleeveRot.Vector()); }
}

bool UViewModelComponent::BuildRealTorch(USceneComponent* Group, UStaticMesh* Mesh, bool bUV)
{
	if (!Mesh)
	{
		return false;
	}
	USceneComponent* Holder = NewObject<USceneComponent>(GetOwner(), bUV ? TEXT("VM_UVTorchHolder") : TEXT("VM_TorchHolder"));
	Holder->SetupAttachment(Group);
	Holder->SetRelativeLocationAndRotation(TorchHoldLoc, TorchHoldRot);
	Holder->RegisterComponent();

	// Real size (23 cm long, 5.5 cm across), held round the tail half of the body, head forward.
	AddMesh(Holder, Mesh, FVector(TorchX, 0.f, 0.f), ViewModelAssets::TorchForward());
	if (bUV)
	{
		// Violet glow behind the glass so it reads as a UV lamp even with the beam off.
		AddPart(Holder, CylinderMesh, FVector(TorchX + 11.0f, 0.f, 0.f), FRotator(90.f, 0.f, 0.f), FVector(3.7f, 3.7f, 0.12f), ViewModelColors::UVLens);
	}
	AddHand(Holder, FVector(-2.f, 0.f, 0.f), FRotator(-90.f, 0.f, 0.f), 1.0f, 2.75f, true);
	AddSleeve(Holder, FVector(-8.5f, 2.8f, -5.5f), FVector(-0.8f, 0.3f, -0.45f));
	return true;
}

bool UViewModelComponent::BuildRealGunTorch(USceneComponent* Group)
{
	for (const UStaticMesh* M : PistolMeshes)
	{
		if (!M)
		{
			return false;
		}
	}
	// Bore along +X, top +Z, origin just above the grip (real size: 22 cm long).
	USceneComponent* Gun = NewObject<USceneComponent>(GetOwner(), TEXT("VM_Pistol"));
	Gun->SetupAttachment(Group);
	Gun->SetRelativeLocationAndRotation(GunLoc, GunRot);
	Gun->RegisterComponent();

	for (int32 i = 0; i < PistolMeshes.Num(); ++i)
	{
		UStaticMeshComponent* P = AddMesh(Gun, PistolMeshes[i], FVector::ZeroVector, FRotator::ZeroRotator);
		if (i == ViewModelAssets::PistolSlideIndex)
		{
			PistolSlide = P;
			PistolSlideRest = P->GetRelativeLocation();
		}
	}

	// Weapon light clamped under the barrel: the torch model shrunk to a compact light, anodised black.
	AddPart(Gun, CubeMesh, LightLoc + FVector(-1.f, 0.f, 1.25f), FRotator::ZeroRotator, FVector(6.f, 1.9f, 1.f), ViewModelColors::Metal);       // rail clamp
	AddPart(Gun, CubeMesh, LightLoc + FVector(-3.6f, 0.f, 0.f), FRotator::ZeroRotator, FVector(1.2f, 1.6f, 1.6f), ViewModelColors::Rubber);    // pressure switch
	if (WeaponLightMesh)
	{
		AddMesh(Gun, WeaponLightMesh, LightLoc, ViewModelAssets::TorchForward(true), LightScale);
	}
	else if (TorchMesh)
	{
		UStaticMeshComponent* Light = AddMesh(Gun, TorchMesh, LightLoc, ViewModelAssets::TorchForward(true), LightScale);
		if (UMaterialInstanceDynamic* Mat = Light->CreateDynamicMaterialInstance(0))
		{
			Mat->SetVectorParameterValue(TEXT("BaseColorFactor"), ViewModelColors::WeaponLight);
		}
	}
	else
	{
		const FRotator Fwd(90.f, 0.f, 0.f);
		AddPart(Gun, CylinderMesh, LightLoc, Fwd, FVector(2.4f, 2.4f, 7.f), ViewModelColors::Metal);
		AddPart(Gun, CylinderMesh, LightLoc + FVector(4.f, 0.f, 0.f), Fwd, FVector(2.9f, 2.9f, 1.6f), ViewModelColors::Metal);
		AddPart(Gun, CylinderMesh, LightLoc + FVector(4.85f, 0.f, 0.f), Fwd, FVector(2.4f, 2.4f, 0.2f), ViewModelColors::Lens);
	}

	AddHand(Gun, HandLoc, HandRot, 0.85f, 2.3f);
	AddSleeve(Gun, SleeveLoc, SleeveDir);
	return true;
}

void UViewModelComponent::AddRoundedSlab(USceneComponent* Parent, const FTransform& Dev, const FVector& Center, float Depth,
	float Width, float Height, float Radius, const FLinearColor& Color)
{
	// A slab with rounded corners seen from the front (-X): two crossed boxes plus four corner cylinders.
	const FRotator Rot = Dev.Rotator();
	const FRotator AlongX = (Dev.GetRotation() * FQuat(FRotator(90.f, 0.f, 0.f))).Rotator();
	const float R = FMath::Min(Radius, FMath::Min(Width, Height) * 0.5f);
	AddPart(Parent, CubeMesh, Dev.TransformPosition(Center), Rot, FVector(Depth, Width, Height - 2.f * R), Color);
	AddPart(Parent, CubeMesh, Dev.TransformPosition(Center), Rot, FVector(Depth, Width - 2.f * R, Height), Color);
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector C = Center + FVector(0.f, (i & 1 ? 1.f : -1.f) * (Width * 0.5f - R), (i & 2 ? 1.f : -1.f) * (Height * 0.5f - R));
		AddPart(Parent, CylinderMesh, Dev.TransformPosition(C), AlongX, FVector(2.f * R, 2.f * R, Depth), Color);
	}
}

void UViewModelComponent::BuildThermometer(USceneComponent* Group)
{
	// Handheld digital thermometer (thermocouple type): yellow rubber holster, grey face, LCD,
	// probe out of the top. Stands up in the hand, face towards the camera, top tipped away a little.
	const FRotator DevRot(-26.f, -4.f, 0.f);
	const FTransform Dev(DevRot, DeviceOffset);
	const FRotator AlongX = (FQuat(DevRot) * FQuat(FRotator(90.f, 0.f, 0.f))).Rotator();
	auto P = [&](UStaticMesh* M, const FVector& L, const FRotator& R, const FVector& S, const FLinearColor& C, UMaterialInstanceDynamic** Out = nullptr)
	{
		AddPart(Group, M, Dev.TransformPosition(L), R, S, C, Out);
	};
	AddRoundedSlab(Group, Dev, FVector(0.f, 0.f, 1.5f), 3.4f, 7.4f, 15.f, 1.5f, ViewModelColors::Holster);    // holster
	AddRoundedSlab(Group, Dev, FVector(-1.75f, 0.f, 1.9f), 0.2f, 6.0f, 12.6f, 1.0f, ViewModelColors::Face);    // face plate
	P(CubeMesh, FVector(-1.9f, 0.f, 5.1f), DevRot, FVector(0.15f, 5.0f, 3.9f), ViewModelColors::Rubber);       // LCD bezel
	UMaterialInstanceDynamic* ScreenMat = nullptr;
	P(CubeMesh, FVector(-1.98f, 0.f, 5.1f), DevRot, FVector(0.1f, 4.4f, 3.2f), ViewModelColors::ScreenWarm, &ScreenMat); // LCD
	ThermoScreen = ScreenMat;
	// Round buttons: power (yellow) and two greys, then a row of small ones.
	const FLinearColor Btn[3] = { ViewModelColors::Holster, ViewModelColors::ButtonGrey, ViewModelColors::ButtonGrey };
	for (int32 i = 0; i < 3; ++i)
	{
		P(CylinderMesh, FVector(-1.95f, -1.8f + i * 1.8f, 1.3f), AlongX, FVector(1.25f, 1.25f, 0.35f), Btn[i]);
		P(CylinderMesh, FVector(-1.95f, -1.8f + i * 1.8f, -0.6f), AlongX, FVector(0.9f, 0.9f, 0.3f), ViewModelColors::ButtonGrey);
	}
	P(CubeMesh, FVector(-1.9f, 0.f, -3.0f), DevRot, FVector(0.12f, 3.6f, 0.5f), ViewModelColors::Brass);       // label strip
	// Probe: socket, black handle collar and a steel rod.
	P(CylinderMesh, FVector(0.f, 0.f, 9.4f), DevRot, FVector(2.0f, 2.0f, 1.4f), ViewModelColors::Rubber);
	P(CylinderMesh, FVector(0.f, 0.f, 10.6f), DevRot, FVector(1.2f, 1.2f, 1.4f), ViewModelColors::Rubber);
	P(CylinderMesh, FVector(0.f, 0.f, 14.2f), DevRot, FVector(0.45f, 0.45f, 6.f), ViewModelColors::MetalLight);
	if (SphereMesh)
	{
		P(SphereMesh, FVector(0.f, 0.f, 17.2f), DevRot, FVector(0.5f, 0.5f, 0.5f), ViewModelColors::MetalLight);
	}
	AddHand(Group, Dev.TransformPosition(FVector(0.3f, 0.f, -4.0f)), DevRot, 0.9f, 2.4f);
	AddSleeve(Group, Dev.TransformPosition(FVector(1.5f, 3.f, -10.f)), FVector(-0.45f, 0.35f, -0.82f));
}

void UViewModelComponent::BuildEMF(USceneComponent* Group)
{
	// K-II style EMF meter: slim black body, a row of five LEDs (green to red) at the top, label below.
	const FRotator DevRot(-26.f, -4.f, 0.f);
	const FTransform Dev(DevRot, DeviceOffset);
	const FRotator AlongX = (FQuat(DevRot) * FQuat(FRotator(90.f, 0.f, 0.f))).Rotator();
	auto P = [&](UStaticMesh* M, const FVector& L, const FRotator& R, const FVector& S, const FLinearColor& C, UMaterialInstanceDynamic** Out = nullptr)
	{
		AddPart(Group, M, Dev.TransformPosition(L), R, S, C, Out);
	};
	AddRoundedSlab(Group, Dev, FVector(0.f, 0.f, 2.5f), 2.8f, 6.4f, 15.6f, 1.8f, ViewModelColors::EMFBody);   // body
	AddRoundedSlab(Group, Dev, FVector(-1.45f, 0.f, 7.6f), 0.15f, 5.6f, 2.0f, 0.8f, ViewModelColors::Rubber);  // LED window
	AddRoundedSlab(Group, Dev, FVector(-1.45f, 0.f, 3.4f), 0.12f, 4.8f, 4.2f, 0.5f, ViewModelColors::Label);   // label
	P(CubeMesh, FVector(-1.53f, 0.f, 4.3f), DevRot, FVector(0.06f, 3.6f, 0.9f), ViewModelColors::LabelInk);    // printed "K-II"
	P(CubeMesh, FVector(-1.53f, 0.f, 2.6f), DevRot, FVector(0.06f, 3.0f, 0.35f), ViewModelColors::LabelInk);
	P(CylinderMesh, FVector(-1.5f, 0.f, -0.6f), AlongX, FVector(1.3f, 1.3f, 0.35f), ViewModelColors::ButtonGrey); // push switch

	EMFLeds.Reset();
	for (int32 i = 0; i < 5; ++i)
	{
		UMaterialInstanceDynamic* Mat = nullptr;
		P(CylinderMesh, FVector(-1.6f, -2.0f + i * 1.0f, 7.6f), AlongX, FVector(0.75f, 0.75f, 0.25f), FLinearColor::Black, &Mat);
		EMFLeds.Add(Mat);
	}
	AddHand(Group, Dev.TransformPosition(FVector(0.3f, 0.f, -3.6f)), DevRot, 0.9f, 2.0f);
	AddSleeve(Group, Dev.TransformPosition(FVector(1.5f, 3.f, -9.6f)), FVector(-0.45f, 0.35f, -0.82f));
}

void UViewModelComponent::BuildModels(USceneComponent* Camera)
{
	ViewRoot = NewObject<USceneComponent>(GetOwner(), TEXT("ViewModelRoot"));
	ViewRoot->SetupAttachment(Camera);
	ViewRoot->SetRelativeLocationAndRotation(HoldOffset, HoldRotation);
	ViewRoot->RegisterComponent();

	Models.Reset();
	Models.SetNum(GadgetTable::Num());

	// Imported models where available, otherwise the basic-shape placeholder.
	Models[static_cast<int32>(EGadget::Torch)] = MakeGroup(TEXT("VM_Torch"));
	if (!BuildRealTorch(Models[static_cast<int32>(EGadget::Torch)], TorchMesh, false))
	{
		BuildTorch(Models[static_cast<int32>(EGadget::Torch)], ViewModelColors::Metal, ViewModelColors::Lens);
	}

	Models[static_cast<int32>(EGadget::GunTorch)] = MakeGroup(TEXT("VM_GunTorch"));
	if (!BuildRealGunTorch(Models[static_cast<int32>(EGadget::GunTorch)]))
	{
		BuildGunTorch(Models[static_cast<int32>(EGadget::GunTorch)]);
	}

	Models[static_cast<int32>(EGadget::UVTorch)] = MakeGroup(TEXT("VM_UVTorch"));
	if (!BuildRealTorch(Models[static_cast<int32>(EGadget::UVTorch)], UVTorchMesh, true))
	{
		BuildTorch(Models[static_cast<int32>(EGadget::UVTorch)], ViewModelColors::UVHead, ViewModelColors::UVLens);
	}

	Models[static_cast<int32>(EGadget::Thermometer)] = MakeGroup(TEXT("VM_Thermometer"));
	BuildThermometer(Models[static_cast<int32>(EGadget::Thermometer)]);

	Models[static_cast<int32>(EGadget::EMFMeter)] = MakeGroup(TEXT("VM_EMFMeter"));
	BuildEMF(Models[static_cast<int32>(EGadget::EMFMeter)]);

	for (USceneComponent* Model : Models)
	{
		if (Model)
		{
			Model->SetVisibility(false, true);
		}
	}

	// Tiny warm fill so the hand reads in the dark. Lighting channel 1 only: it never touches the room.
	FillLight = NewObject<UPointLightComponent>(GetOwner(), TEXT("ViewModelFill"));
	FillLight->SetupAttachment(ViewRoot);
	FillLight->SetRelativeLocation(FVector(-14.f, -10.f, 9.f));
	FillLight->SetIntensityUnits(ELightUnits::Candelas);
	FillLight->SetIntensity(FillLightIntensity);
	FillLight->SetAttenuationRadius(FillLightRadius);
	FillLight->SetLightColor(FLinearColor(1.f, 0.78f, 0.55f));
	FillLight->SetCastShadows(false);
	FillLight->bAffectsWorld = true;
	FillLight->SetAffectTranslucentLighting(false);
	FillLight->SetSpecularScale(0.4f);
	FillLight->LightingChannels.bChannel0 = false;
	FillLight->LightingChannels.bChannel1 = true;
	FillLight->RegisterComponent();

	ShownIndex = -2;
}

// ---------------------------------------------------------------- Tick

void UViewModelComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AGhostHunterCharacter* Hunter = GetHunter();
	if (!Hunter || !ViewRoot)
	{
		return;
	}
	const UGadgetComponent* Gadgets = Hunter->GetGadgets();
	const EGadget Equipped = Gadgets ? Gadgets->GetEquipped() : EGadget::Torch;
	const float Flash = Gadgets ? Gadgets->GetFlashAlpha() : 0.f;

	UpdateVisibility(Hunter->IsHiding(), Equipped);
	UpdateMotion(DeltaTime, Hunter, Flash);
	UpdateReadouts(Equipped);
}

void UViewModelComponent::UpdateVisibility(bool bHiding, EGadget Equipped)
{
	const int32 Want = bHiding ? -1 : static_cast<int32>(Equipped);
	if (Want == ShownIndex)
	{
		return;
	}
	ShownIndex = Want;
	for (int32 i = 0; i < Models.Num(); ++i)
	{
		if (Models[i])
		{
			Models[i]->SetVisibility(i == Want, true);
		}
	}
	if (FillLight)
	{
		FillLight->SetVisibility(Want >= 0);
	}
	// Fresh pull-out: start slightly low so the swap reads as raising the gadget.
	if (Want >= 0)
	{
		BobWeight = 0.f;
		Recoil = -0.6f;
		RecoilVel = 0.f;
	}
}

void UViewModelComponent::UpdateMotion(float DeltaTime, AGhostHunterCharacter* Hunter, float Flash)
{
	const float Dt = FMath::Min(DeltaTime, 0.1f);

	// ---- Recoil: the flash value jumps to 1 on a Gun Torch shot.
	if (Flash > LastFlash + 0.3f)
	{
		RecoilVel += 22.f;
	}
	LastFlash = Flash;
	// Critically-damped-ish spring back to rest.
	const float Stiffness = 160.f;
	const float Damping = 20.f;
	RecoilVel += (-Stiffness * Recoil - Damping * RecoilVel) * Dt;
	Recoil += RecoilVel * Dt;

	// ---- Walk bob from horizontal speed.
	const FVector Vel = Hunter->GetVelocity();
	const float Speed = FVector(Vel.X, Vel.Y, 0.f).Size();
	const float TargetWeight = FMath::Clamp(Speed / 250.f, 0.f, 1.6f);
	BobWeight = FMath::FInterpTo(BobWeight, TargetWeight, Dt, 6.f);
	BobPhase += Dt * (6.5f + Speed * 0.012f);
	if (BobPhase > 2.f * PI * 100.f)
	{
		BobPhase -= 2.f * PI * 100.f;
	}

	// ---- Idle breathing sway.
	SwayTime += Dt;
	const float Breath = FMath::Sin(SwayTime * 1.3f);
	const float Drift = FMath::Sin(SwayTime * 0.7f + 1.1f);

	// ---- Look lag: the hand trails the view a little when turning.
	if (const AController* C = Hunter->GetController())
	{
		const FRotator Rot = C->GetControlRotation();
		if (bHaveControlRot && Dt > KINDA_SMALL_NUMBER)
		{
			const FRotator Delta = (Rot - LastControlRot).GetNormalized();
			LookLag.Yaw = FMath::Clamp(LookLag.Yaw - Delta.Yaw * 0.25f, -4.f, 4.f);
			LookLag.Pitch = FMath::Clamp(LookLag.Pitch - Delta.Pitch * 0.25f, -3.f, 3.f);
		}
		LastControlRot = Rot;
		bHaveControlRot = true;
	}
	LookLag = FMath::RInterpTo(LookLag, FRotator::ZeroRotator, Dt, 8.f);

	FVector Offset = HoldOffset;
	Offset.Z += Breath * IdleSwayAmount;
	Offset.Y += Drift * IdleSwayAmount * 0.6f;
	Offset.Y += FMath::Sin(BobPhase) * WalkBobAmount * BobWeight;
	Offset.Z += -FMath::Abs(FMath::Cos(BobPhase)) * WalkBobAmount * 1.2f * BobWeight;
	Offset.X += -Recoil * 3.5f;
	Offset.Z += Recoil * 1.2f;
	Offset.Y += LookLag.Yaw * 0.35f;
	Offset.Z += LookLag.Pitch * 0.35f;

	FRotator Rot = HoldRotation;
	Rot.Pitch += Recoil * 9.f + Breath * IdleSwayAmount * 0.6f;
	Rot.Yaw += LookLag.Yaw + FMath::Sin(BobPhase) * 0.6f * BobWeight;
	Rot.Roll += Drift * IdleSwayAmount + FMath::Sin(BobPhase) * 1.2f * BobWeight;
	Rot.Pitch += LookLag.Pitch;

	ViewRoot->SetRelativeLocationAndRotation(Offset, Rot);

	if (PistolSlide)
	{
		// The slide cycles back on a shot and springs home with the recoil.
		const float Back = FMath::Clamp(Recoil * 3.4f, 0.f, 2.6f);
		PistolSlide->SetRelativeLocation(PistolSlideRest - FVector(Back, 0.f, 0.f));
	}

	if (FillLight)
	{
		// Shot flash briefly lights the hand.
		FillLight->SetIntensity(FillLightIntensity * (1.f + Flash * 6.f));
	}
}

void UViewModelComponent::UpdateReadouts(EGadget Equipped)
{
	const AGhostHunterCharacter* Hunter = GetHunter();
	const UGadgetComponent* Gadgets = Hunter ? Hunter->GetGadgets() : nullptr;
	if (!Gadgets)
	{
		return;
	}

	if (Equipped == EGadget::EMFMeter && EMFLeds.Num() == 5)
	{
		const int32 Level = FMath::Clamp(Gadgets->GetEMFLevel(), 0, 5);
		if (Level != LastEMFLevel)
		{
			LastEMFLevel = Level;
			static const FLinearColor LedColors[5] = {
				FLinearColor(0.1f, 1.f, 0.1f), FLinearColor(0.55f, 1.f, 0.05f), FLinearColor(1.f, 0.9f, 0.05f),
				FLinearColor(1.f, 0.45f, 0.02f), FLinearColor(1.f, 0.05f, 0.03f) };
			for (int32 i = 0; i < 5; ++i)
			{
				if (EMFLeds[i])
				{
					EMFLeds[i]->SetVectorParameterValue(TEXT("Color"), i < Level ? LedColors[i] : LedColors[i] * 0.06f);
				}
			}
		}
	}
	else if (Equipped == EGadget::Thermometer && ThermoScreen)
	{
		const int32 Cold = Gadgets->GetTemperature() < 10.f ? 1 : 0;
		if (Cold != LastThermoCold)
		{
			LastThermoCold = Cold;
			ThermoScreen->SetVectorParameterValue(TEXT("Color"), Cold ? ViewModelColors::ScreenCold : ViewModelColors::ScreenWarm);
		}
	}
}
