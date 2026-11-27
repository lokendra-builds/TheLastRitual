#include "GhostHuntHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/GameInstance.h"
#include "Gadgets.h"
#include "GadgetComponent.h"
#include "ClueComponent.h"
#include "RoomArea.h"
#include "ClueActor.h"
#include "RecordingSubsystem.h"
#include "Engine/Texture2D.h"
#include "ProgressSubsystem.h"
#include "EngineUtils.h"
#include "GhostBase.h"
#include "GhostDefinition.h"
#include "GhostHunterCharacter.h"
#include "NightSubsystem.h"
#include "TorchComponent.h"

#define LOCTEXT_NAMESPACE "GhostHunt"

namespace
{
	const FLinearColor Paper(0.95f, 0.92f, 0.85f, 0.95f);
	const FLinearColor Faded(0.95f, 0.92f, 0.85f, 0.55f);
	const FLinearColor Ink(0.85f, 0.65f, 0.35f, 1.f);
}

void AGhostHuntHUD::BeginPlay()
{
	Super::BeginPlay();

	if (UGhostGuideSubsystem* Guide = GetWorld()->GetSubsystem<UGhostGuideSubsystem>())
	{
		Guide->OnClueRecorded.AddDynamic(this, &AGhostHuntHUD::HandleClueRecorded);
	}
	if (UNightSubsystem* Night = GetWorld()->GetSubsystem<UNightSubsystem>())
	{
		Night->OnRitualPerformed.AddDynamic(this, &AGhostHuntHUD::HandleRitualPerformed);
	}
}

void AGhostHuntHUD::HandleRitualPerformed(FName RitualId, bool bCorrect)
{
	const UNightSubsystem* Night = GetWorld()->GetSubsystem<UNightSubsystem>();
	if (!bCorrect && Night && Night->GetLives() > 0)
	{
		ShowMessage(Night->GetLives() == 1
			? LOCTEXT("WrongLast", "Wrong ritual. She is furious. One life left.")
			: LOCTEXT("Wrong", "Wrong ritual. The spirit grows angrier."), 4.f);
	}
}

void AGhostHuntHUD::ToggleMenu()
{
	if (bShopOpen)
	{
		SetShopOpen(false); // Esc closes the shop first
		return;
	}
	SetMenuOpen(!bMenuOpen);
}

void AGhostHuntHUD::SetMenuOpen(bool bOpen)
{
	bMenuOpen = bOpen;
	if (bOpen)
	{
		bShopOpen = false;
	}
	ApplyCursorMode();
}

void AGhostHuntHUD::ToggleShop()
{
	SetShopOpen(!bShopOpen);
}

void AGhostHuntHUD::SetShopOpen(bool bOpen)
{
	bShopOpen = bOpen;
	if (bOpen)
	{
		bMenuOpen = false;
	}
	ApplyCursorMode();
}

void AGhostHuntHUD::ApplyCursorMode()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		return;
	}

	const bool bCursor = bMenuOpen || bShopOpen;
	HoveredButton = NAME_None;
	PC->SetPause(bCursor);
	PC->bShowMouseCursor = bCursor;
	PC->bEnableClickEvents = bCursor;
	PC->bEnableMouseOverEvents = bCursor;

	if (bCursor)
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(Mode);
	}
	else
	{
		PC->SetInputMode(FInputModeGameOnly());
	}
}

// Simple pictograms drawn from rectangles and lines (real icons come with the art pass).
void AGhostHuntHUD::DrawGadgetIcon(uint8 Gadget, float X, float Y, float S, const FLinearColor& C)
{
	if (GadgetIcons.Num() == 0)
	{
		static const TCHAR* Names[] = { TEXT("Torch"), TEXT("GunTorch"), TEXT("UVTorch"), TEXT("Thermometer"), TEXT("EMFMeter") };
		for (const TCHAR* Name : Names)
		{
			GadgetIcons.Add(LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/UI/Icons/T_Icon_%s.T_Icon_%s"), Name, Name)));
		}
	}
	if (GadgetIcons.IsValidIndex(Gadget) && GadgetIcons[Gadget])
	{
		// Locked gadgets are drawn dim and grey; owned ones in full colour.
		const FLinearColor Tint = C.A < 0.5f ? FLinearColor(0.45f, 0.45f, 0.45f, 0.45f) : FLinearColor::White;
		DrawTexture(GadgetIcons[Gadget], X, Y, S, S, 0.f, 0.f, 1.f, 1.f, Tint, BLEND_Translucent);
		return;
	}

	const FLinearColor Dark(C.R * 0.35f, C.G * 0.35f, C.B * 0.35f, C.A);
	auto R = [&](float x, float y, float w, float h, const FLinearColor& Col) { DrawRect(Col, X + x * S, Y + y * S, w * S, h * S); };
	auto L = [&](float x0, float y0, float x1, float y1) { DrawLine(X + x0 * S, Y + y0 * S, X + x1 * S, Y + y1 * S, C, FMath::Max(1.f, S * 0.04f)); };

	switch (static_cast<EGadget>(Gadget))
	{
	case EGadget::Torch:
	case EGadget::UVTorch:
		R(0.08f, 0.42f, 0.42f, 0.16f, Dark);   // handle
		R(0.48f, 0.36f, 0.14f, 0.28f, C);      // head
		L(0.64f, 0.38f, 0.95f, 0.18f); L(0.64f, 0.5f, 0.98f, 0.5f); L(0.64f, 0.62f, 0.95f, 0.82f); // beam
		break;
	case EGadget::GunTorch:
		R(0.05f, 0.34f, 0.62f, 0.14f, C);      // barrel
		R(0.12f, 0.46f, 0.14f, 0.32f, Dark);   // grip
		R(0.32f, 0.48f, 0.26f, 0.1f, Dark);    // torch under the barrel
		L(0.6f, 0.53f, 0.95f, 0.42f); L(0.6f, 0.53f, 0.95f, 0.64f);
		break;
	case EGadget::Thermometer:
		R(0.42f, 0.08f, 0.16f, 0.66f, Dark);   // tube
		R(0.45f, 0.34f, 0.1f, 0.42f, C);       // mercury
		R(0.34f, 0.72f, 0.32f, 0.22f, C);      // bulb
		break;
	case EGadget::EMFMeter:
		R(0.2f, 0.1f, 0.6f, 0.82f, Dark);      // body
		for (int32 i = 0; i < 5; ++i)
		{
			R(0.28f + i * 0.09f, 0.62f - i * 0.08f, 0.06f, 0.12f + i * 0.08f, C); // bars
		}
		break;
	default:
		break;
	}
}

void AGhostHuntHUD::DrawHotbar(UFont* Font)
{
	const AGhostHunterCharacter* Hunter = Cast<AGhostHunterCharacter>(GetOwningPawn());
	const UGadgetComponent* G = Hunter ? Hunter->GetGadgets() : nullptr;
	if (!G)
	{
		return;
	}

	const int32 N = GadgetTable::Num();
	const float Slot = 54.f, Gap = 8.f;
	const float StartX = (Canvas->ClipX - (N * Slot + (N - 1) * Gap)) * 0.5f;
	const float Y = Canvas->ClipY - Slot - 26.f;

	for (int32 i = 0; i < N; ++i)
	{
		const EGadget Gadget = static_cast<EGadget>(i);
		const bool bOwned = G->IsOwned(Gadget);
		const bool bEquipped = G->GetEquipped() == Gadget;
		const FGadgetInfo& Info = GadgetTable::Info(Gadget);
		const float X = StartX + i * (Slot + Gap);

		DrawRect(bEquipped ? FLinearColor(0.35f, 0.12f, 0.05f, 0.85f) : FLinearColor(0.f, 0.f, 0.f, 0.5f), X, Y, Slot, Slot);
		if (bEquipped)
		{
			DrawRect(Ink, X, Y + Slot - 3.f, Slot, 3.f);
		}
		FLinearColor C = Info.Color;
		C.A = bOwned ? 1.f : 0.25f;
		DrawGadgetIcon(static_cast<uint8>(i), X + 7.f, Y + 7.f, Slot - 14.f, C);
		DrawText(FString::Printf(TEXT("%d"), i + 1), bOwned ? Paper : Faded, X + 4.f, Y + 2.f, Font, 0.7f);
		if (!bOwned)
		{
			DrawText(TEXT("B"), Faded, X + Slot - 14.f, Y + Slot - 18.f, Font, 0.7f); // buy in the shop
		}
	}

	const FString Name = GadgetTable::Info(G->GetEquipped()).Name;
	float TW = 0.f, TH = 0.f;
	GetTextSize(Name, TW, TH, Font, 0.85f);
	DrawText(Name, Paper, (Canvas->ClipX - TW) * 0.5f, Y - 22.f, Font, 0.85f);
}

void AGhostHuntHUD::DrawGadgetReadout(UFont* Font)
{
	const AGhostHunterCharacter* Hunter = Cast<AGhostHunterCharacter>(GetOwningPawn());
	const UGadgetComponent* G = Hunter ? Hunter->GetGadgets() : nullptr;
	if (!G)
	{
		return;
	}

	// Muzzle flash + hit marker.
	if (G->GetFlashAlpha() > 0.f)
	{
		DrawRect(FLinearColor(1.f, 0.95f, 0.85f, 0.22f * G->GetFlashAlpha()), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	}
	const float CX = Canvas->ClipX * 0.5f, CY = Canvas->ClipY * 0.5f;
	if (G->WasRecentHit())
	{
		const FLinearColor Red(1.f, 0.2f, 0.1f, 1.f);
		DrawLine(CX - 14.f, CY - 14.f, CX - 6.f, CY - 6.f, Red, 2.f);
		DrawLine(CX + 14.f, CY - 14.f, CX + 6.f, CY - 6.f, Red, 2.f);
		DrawLine(CX - 14.f, CY + 14.f, CX - 6.f, CY + 6.f, Red, 2.f);
		DrawLine(CX + 14.f, CY + 14.f, CX + 6.f, CY + 6.f, Red, 2.f);
	}

	// Reading panel, bottom-left above the torch bar (the held model covers the bottom-right).
	const float W = 200.f, H = 64.f;
	const float X = 40.f, Y = Canvas->ClipY - H - 92.f;
	switch (G->GetEquipped())
	{
	case EGadget::GunTorch:
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), X, Y, W, H);
		DrawText(TEXT("GUN TORCH"), Faded, X + 10.f, Y + 6.f, Font, 0.75f);
		DrawText(FString::Printf(TEXT("Shots  %d / %d"), G->GetShotsLeft(), G->ShotsPerNight), Paper, X + 10.f, Y + 28.f, Font, 1.1f);
		break;
	case EGadget::Thermometer:
	{
		const float T = G->GetTemperature();
		const FLinearColor C = T < 5.f ? FLinearColor(0.4f, 0.7f, 1.f) : (T < 14.f ? FLinearColor(0.7f, 0.9f, 1.f) : Paper);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), X, Y, W, H);
		DrawText(TEXT("THERMOMETER"), Faded, X + 10.f, Y + 6.f, Font, 0.75f);
		DrawText(FString::Printf(TEXT("%.1f C%s"), T, T < 5.f ? TEXT("   cold spot!") : TEXT("")), C, X + 10.f, Y + 26.f, Font, 1.3f);
		break;
	}
	case EGadget::EMFMeter:
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), X, Y, W, H);
		DrawText(FString::Printf(TEXT("EMF  level %d"), G->GetEMFLevel()), Faded, X + 10.f, Y + 6.f, Font, 0.75f);
		for (int32 i = 0; i < 5; ++i)
		{
			const bool bLit = i < G->GetEMFLevel();
			const FLinearColor C = i < 2 ? FLinearColor(0.3f, 1.f, 0.4f) : (i < 4 ? FLinearColor(1.f, 0.8f, 0.2f) : FLinearColor(1.f, 0.2f, 0.1f));
			DrawRect(bLit ? C : FLinearColor(0.15f, 0.15f, 0.15f, 0.8f), X + 10.f + i * 37.f, Y + 30.f, 30.f, 24.f);
		}
		break;
	}
	case EGadget::UVTorch:
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), X, Y, W, H);
		DrawText(TEXT("UV TORCH"), Faded, X + 10.f, Y + 6.f, Font, 0.75f);
		DrawText(TEXT("Sweep walls and floors"), FLinearColor(0.75f, 0.55f, 1.f), X + 10.f, Y + 30.f, Font, 0.9f);
		break;
	default:
		break;
	}
}

void AGhostHuntHUD::DrawGhostHealth(UFont* Font)
{
	TActorIterator<AGhostBase> It(GetWorld());
	const AGhostBase* Ghost = It ? *It : nullptr;
	if (!Ghost || !Ghost->CanBeHit())
	{
		return;
	}
	const float W = 320.f, H = 8.f;
	const float X = (Canvas->ClipX - W) * 0.5f, Y = 84.f;
	DrawText(TEXT("SPIRIT"), FLinearColor(1.f, 0.3f, 0.25f, 0.8f), X, Y - 18.f, Font, 0.75f);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), X, Y, W, H);
	DrawRect(FLinearColor(0.75f, 0.08f, 0.05f, 0.95f), X, Y, W * Ghost->GetHealthPercent(), H);
}

// ---------------------------------------------------------------- Compass & map
//
// Directions follow the map: north = +Y (the prayer room side), east = -X
// (to your right when you face north). Headings are degrees clockwise from north.

namespace
{
	float HeadingOf(const FVector& Dir)
	{
		float H = FMath::RadiansToDegrees(FMath::Atan2(-Dir.X, Dir.Y));
		return H < 0.f ? H + 360.f : H;
	}

	float WrapDelta(float A)
	{
		while (A > 180.f) A -= 360.f;
		while (A < -180.f) A += 360.f;
		return A;
	}
}

const ARoomArea* AGhostHuntHUD::RoomAt(const FVector& Point) const
{
	for (TActorIterator<ARoomArea> It(GetWorld()); It; ++It)
	{
		if (It->ContainsPoint(Point))
		{
			return *It;
		}
	}
	return nullptr;
}

void AGhostHuntHUD::UpdateNavTargets()
{
	const APawn* Player = GetOwningPawn();
	if (!Player)
	{
		return;
	}
	CurrentRoom = RoomAt(Player->GetActorLocation());

	// Objective: the nearest unfound sign in this room, otherwise the nearest room that has one.
	bHasObjective = false;
	bObjectiveInRoom = false;
	ObjectiveRoom = nullptr;
	const UGhostGuideSubsystem* Guide = GetWorld()->GetSubsystem<UGhostGuideSubsystem>();
	if (Guide && Guide->GetFoundCluesRef().Num() >= 3)
	{
		return; // enough to decide: the player works out the rest in the Ghost Guide
	}

	auto IsOpenSign = [](const AActor* A)
	{
		const UClueComponent* Clue = A->FindComponentByClass<UClueComponent>();
		if (!Clue || Clue->SignId.IsNone() || Clue->IsFound())
		{
			return false;
		}
		const AClueActor* ClueActor = Cast<AClueActor>(A);
		return !(ClueActor && ClueActor->NeedsUVReveal()); // invisible until UV: don't point at thin air
	};

	// 1) A sign in the room you're standing in: point right at it.
	float Best = TNumericLimits<float>::Max();
	if (CurrentRoom.IsValid())
	{
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (IsOpenSign(*It) && CurrentRoom->ContainsPoint(It->GetActorLocation()))
			{
				const float D = FVector::Dist(It->GetActorLocation(), Player->GetActorLocation());
				if (D < Best)
				{
					Best = D;
					ObjectiveLocation = It->GetActorLocation();
				}
			}
		}
		if (Best < TNumericLimits<float>::Max())
		{
			bHasObjective = true;
			bObjectiveInRoom = true;
			ObjectiveRoom = CurrentRoom;
			ObjectiveLabel = TEXT("Sign nearby");
			return;
		}
	}

	// 2) Otherwise the nearest room that still hides one.
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (!IsOpenSign(*It))
		{
			continue;
		}
		const ARoomArea* Room = RoomAt(It->GetActorLocation());
		if (!Room || Room == CurrentRoom.Get())
		{
			continue;
		}
		const float D = FVector::Dist2D(Room->GetActorLocation(), Player->GetActorLocation());
		if (D < Best)
		{
			Best = D;
			ObjectiveRoom = Room;
			ObjectiveLocation = Room->GetActorLocation();
			ObjectiveLabel = Room->RoomName.ToString();
			bHasObjective = true;
		}
	}
}

void AGhostHuntHUD::DrawCompass(UFont* Font)
{
	const APlayerController* PC = GetOwningPlayerController();
	const APawn* Player = GetOwningPawn();
	if (!PC || !Player)
	{
		return;
	}

	const float W = FMath::Min(560.f, Canvas->ClipX * 0.42f), H = 30.f;
	const float CX = Canvas->ClipX * 0.5f, Y = 14.f;
	const float HalfFov = 90.f;
	const float PxPerDeg = (W * 0.5f) / HalfFov;
	const float Heading = HeadingOf(PC->GetControlRotation().Vector());

	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), CX - W * 0.5f, Y, W, H);

	static const TCHAR* Labels[] = { TEXT("N"), TEXT("NE"), TEXT("E"), TEXT("SE"), TEXT("S"), TEXT("SW"), TEXT("W"), TEXT("NW") };
	for (int32 Deg = 0; Deg < 360; Deg += 15)
	{
		const float Rel = WrapDelta(Deg - Heading);
		if (FMath::Abs(Rel) > HalfFov)
		{
			continue;
		}
		const float X = CX + Rel * PxPerDeg;
		const float Fade = 1.f - FMath::Abs(Rel) / HalfFov * 0.7f;
		if (Deg % 45 == 0)
		{
			const TCHAR* L = Labels[Deg / 45];
			float TW = 0.f, TH = 0.f;
			GetTextSize(L, TW, TH, Font, 0.9f);
			const FLinearColor C = Deg == 0 ? FLinearColor(0.95f, 0.35f, 0.25f, Fade) : FLinearColor(Paper.R, Paper.G, Paper.B, Fade);
			DrawText(L, C, X - TW * 0.5f, Y + 6.f, Font, 0.9f);
		}
		else
		{
			DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.35f * Fade), X - 0.5f, Y + 20.f, 1.f, 6.f);
		}
	}
	DrawRect(Ink, CX - 1.f, Y + H - 4.f, 2.f, 4.f); // centre mark

	// Objective room marker.
	if (bHasObjective)
	{
		const FVector To = ObjectiveLocation - Player->GetActorLocation();
		const float Rel = WrapDelta(HeadingOf(To) - Heading);
		const float Dist = To.Size2D() / 100.f;
		const bool bOff = FMath::Abs(Rel) > HalfFov;
		const float X = CX + FMath::Clamp(Rel, -HalfFov, HalfFov) * PxPerDeg;
		const FLinearColor Mark(1.f, 0.75f, 0.3f, 1.f);
		// Diamond.
		DrawLine(X, Y + H + 2.f, X + 6.f, Y + H + 8.f, Mark, 2.f);
		DrawLine(X + 6.f, Y + H + 8.f, X, Y + H + 14.f, Mark, 2.f);
		DrawLine(X, Y + H + 14.f, X - 6.f, Y + H + 8.f, Mark, 2.f);
		DrawLine(X - 6.f, Y + H + 8.f, X, Y + H + 2.f, Mark, 2.f);
		const FString Label = FString::Printf(TEXT("%s%s  %.0f m%s"), bOff && Rel < 0 ? TEXT("< ") : TEXT(""),
			*ObjectiveLabel, Dist, bOff && Rel > 0 ? TEXT(" >") : TEXT(""));
		float TW = 0.f, TH = 0.f;
		GetTextSize(Label, TW, TH, Font, 0.8f);
		const float LX = FMath::Clamp(X - TW * 0.5f, CX - W * 0.5f, CX + W * 0.5f - TW);
		DrawText(Label, Mark, LX, Y + H + 16.f, Font, 0.8f);
	}

	// Where am I.
	const FString Here = CurrentRoom.IsValid() ? CurrentRoom->RoomName.ToString() : FString(TEXT("Outside"));
	float TW = 0.f, TH = 0.f;
	GetTextSize(Here, TW, TH, Font, 0.85f);
	DrawText(Here, Faded, CX + W * 0.5f + 12.f, Y + 7.f, Font, 0.85f);
	if (!bHasObjective)
	{
		const UGhostGuideSubsystem* Guide = GetWorld()->GetSubsystem<UGhostGuideSubsystem>();
		if (Guide && Guide->GetFoundCluesRef().Num() >= 3)
		{
			const FString Hint = TEXT("Enough signs. Decide in the Ghost Guide [Tab]");
			GetTextSize(Hint, TW, TH, Font, 0.8f);
			DrawText(Hint, FLinearColor(1.f, 0.75f, 0.3f, 0.9f), CX - TW * 0.5f, Y + H + 16.f, Font, 0.8f);
		}
	}
}

void AGhostHuntHUD::DrawWaypoint(UFont* Font)
{
	const APlayerController* PC = GetOwningPlayerController();
	const APawn* Player = GetOwningPawn();
	if (!PC || !Player)
	{
		return;
	}
	const FLinearColor Mark(1.f, 0.75f, 0.3f, 1.f);
	const float CX = Canvas->ClipX * 0.5f, CY = Canvas->ClipY * 0.5f;

	if (!bHasObjective)
	{
		return;
	}
	TActorIterator<AGhostBase> HuntIt(GetWorld());
	const bool bHuntOn = HuntIt && HuntIt->IsHunting();
	if (bObjectiveInRoom && CurrentRoom.IsValid() && !bHuntOn)
	{
		const FString T = FString::Printf(TEXT("Search this room: %s"), *CurrentRoom->RoomName.ToString());
		float TW = 0.f, TH = 0.f;
		GetTextSize(T, TW, TH, Font, 1.f);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), CX - TW * 0.5f - 10.f, 76.f, TW + 20.f, TH + 8.f);
		DrawText(T, Mark, CX - TW * 0.5f, 80.f, Font, 1.f);
	}

	// A sign: mark the thing itself. A room: mark its middle, a little above the floor.
	const FVector World = ObjectiveLocation + FVector(0.f, 0.f, bObjectiveInRoom ? 25.f : -30.f);
	const float Dist = FVector::Dist2D(World, Player->GetActorLocation()) / 100.f;
	const FString Label = FString::Printf(TEXT("%s  %.0f m"), *ObjectiveLabel, Dist);

	// In front of the camera and on screen: a diamond right where the room is.
	FVector2D Screen;
	const FVector ViewDir = PC->GetControlRotation().Vector();
	const bool bInFront = FVector::DotProduct(ViewDir, (World - PC->PlayerCameraManager->GetCameraLocation()).GetSafeNormal()) > 0.1f;
	const bool bProjected = bInFront && PC->ProjectWorldLocationToScreen(World, Screen, true);
	const float Margin = 70.f;
	const bool bOnScreen = bProjected && Screen.X > Margin && Screen.X < Canvas->ClipX - Margin && Screen.Y > Margin && Screen.Y < Canvas->ClipY - Margin;

	// Screen-space scale: ProjectWorldLocationToScreen returns viewport pixels; Canvas may differ.
	FVector2D P = Screen;
	if (bOnScreen && GEngine && GEngine->GameViewport)
	{
		FVector2D VP;
		GEngine->GameViewport->GetViewportSize(VP);
		if (VP.X > 0.f) { P.X *= Canvas->ClipX / VP.X; P.Y *= Canvas->ClipY / VP.Y; }
	}

	auto Diamond = [&](float X, float Y, float R, float Thick)
	{
		DrawLine(X, Y - R, X + R, Y, Mark, Thick); DrawLine(X + R, Y, X, Y + R, Mark, Thick);
		DrawLine(X, Y + R, X - R, Y, Mark, Thick); DrawLine(X - R, Y, X, Y - R, Mark, Thick);
	};

	float TW = 0.f, TH = 0.f;
	GetTextSize(Label, TW, TH, Font, 0.95f);
	if (bOnScreen)
	{
		const float Pulse = 1.f + 0.15f * FMath::Sin(GetWorld()->GetTimeSeconds() * 4.f);
		Diamond(P.X, P.Y, 12.f * Pulse, 3.f);
		Diamond(P.X, P.Y, 4.f, 3.f);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), P.X - TW * 0.5f - 6.f, P.Y + 18.f, TW + 12.f, TH + 4.f);
		DrawText(Label, Mark, P.X - TW * 0.5f, P.Y + 20.f, Font, 0.95f);
		return;
	}

	// Off screen: an arrow on the screen edge pointing the way to turn.
	const float Heading = HeadingOf(ViewDir);
	const float Rel = WrapDelta(HeadingOf(World - Player->GetActorLocation()) - Heading); // + = to the right
	if (FMath::Abs(Rel) < 25.f)
	{
		return; // it's ahead, just above/below the view: the floor path and compass already show it
	}
	const float Ang = FMath::DegreesToRadians(Rel);
	FVector2D Dir(FMath::Sin(Ang), -FMath::Cos(Ang));
	// Keep clear of the controls panel (right, 300 px) when it's open, and of the hotbar.
	const float RightReserve = bControlsOpen ? 330.f : 60.f;
	const float RX = Canvas->ClipX * 0.5f - RightReserve - 30.f, RY = Canvas->ClipY * 0.5f - 160.f;
	const float K = FMath::Min(FMath::Abs(Dir.X) > 1e-3f ? RX / FMath::Abs(Dir.X) : 1e6f, FMath::Abs(Dir.Y) > 1e-3f ? RY / FMath::Abs(Dir.Y) : 1e6f);
	const FVector2D Pos(CX + Dir.X * K, CY + Dir.Y * K);
	// A proper arrow: shaft + sharp head, pointing the way to turn.
	const FVector2D Side(-Dir.Y, Dir.X);
	const FVector2D Tip = Pos + Dir * 26.f;
	const FVector2D Tail = Pos - Dir * 26.f;
	const FVector2D HeadL = Tip - Dir * 16.f + Side * 11.f, HeadR = Tip - Dir * 16.f - Side * 11.f;
	DrawLine(Tail.X, Tail.Y, Tip.X, Tip.Y, Mark, 5.f);
	DrawLine(Tip.X, Tip.Y, HeadL.X, HeadL.Y, Mark, 5.f);
	DrawLine(Tip.X, Tip.Y, HeadR.X, HeadR.Y, Mark, 5.f);
	const TCHAR* Way = FMath::Abs(Rel) > 150.f ? TEXT("turn around") : (FMath::Abs(Rel) < 25.f ? TEXT("straight ahead") : (Rel > 0.f ? TEXT("turn right") : TEXT("turn left")));
	const FString Turn = FString::Printf(TEXT("%s  (%s)"), *Label, Way);
	GetTextSize(Turn, TW, TH, Font, 0.9f);
	const float LX = FMath::Clamp(Pos.X - TW * 0.5f, 20.f, Canvas->ClipX - TW - RightReserve);
	const float LY = FMath::Clamp(Pos.Y + (Dir.Y > 0.f ? -44.f : 26.f), 60.f, Canvas->ClipY - 140.f);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), LX - 6.f, LY - 2.f, TW + 12.f, TH + 4.f);
	DrawText(Turn, Mark, LX, LY, Font, 0.9f);
}

void AGhostHuntHUD::DrawGhostDirection(UFont* Font)
{
	TActorIterator<AGhostBase> It(GetWorld());
	const AGhostBase* Ghost = It ? *It : nullptr;
	const APlayerController* PC = GetOwningPlayerController();
	const APawn* Player = GetOwningPawn();
	if (!Ghost || !Ghost->IsHunting() || !PC || !Player)
	{
		return;
	}

	const FVector Threat = Ghost->GetThreatLocation();
	const float Heading = HeadingOf(PC->GetControlRotation().Vector());
	const float Rel = WrapDelta(HeadingOf(Threat - Player->GetActorLocation()) - Heading);
	const float Dist = FVector::Dist2D(Threat, Player->GetActorLocation()) / 100.f;
	const float Pulse = 0.6f + 0.4f * FMath::Sin(GetWorld()->GetTimeSeconds() * 8.f);
	const FLinearColor Red(1.f, 0.15f, 0.1f, Pulse);
	const float CX = Canvas->ClipX * 0.5f, CY = Canvas->ClipY * 0.5f;

	// On the compass: a red mark where she is (clamped to the edge if behind).
	{
		const float W = FMath::Min(560.f, Canvas->ClipX * 0.42f);
		const float X = CX + FMath::Clamp(Rel, -90.f, 90.f) * (W * 0.5f) / 90.f;
		DrawRect(Red, X - 5.f, 14.f, 10.f, 30.f);
	}

	// Already in view and visible: the red edges and her eyes say enough.
	if (!Ghost->IsHuntWarning() && FMath::Abs(Rel) < 35.f)
	{
		return;
	}

	// Big red arrow from the centre ring towards her.
	const float Ang = FMath::DegreesToRadians(Rel);
	const FVector2D Dir(FMath::Sin(Ang), -FMath::Cos(Ang));
	const FVector2D Side(-Dir.Y, Dir.X);
	const float Ring = FMath::Min(Canvas->ClipX, Canvas->ClipY) * 0.2f;
	const FVector2D Base = FVector2D(CX, CY) + Dir * Ring;
	const FVector2D Tip = Base + Dir * 34.f;
	const FVector2D HL = Tip - Dir * 20.f + Side * 14.f, HR = Tip - Dir * 20.f - Side * 14.f;
	DrawLine(Base.X, Base.Y, Tip.X, Tip.Y, Red, 6.f);
	DrawLine(Tip.X, Tip.Y, HL.X, HL.Y, Red, 6.f);
	DrawLine(Tip.X, Tip.Y, HR.X, HR.Y, Red, 6.f);

	const TCHAR* Where = FMath::Abs(Rel) > 135.f ? TEXT("BEHIND YOU") : (FMath::Abs(Rel) < 35.f ? TEXT("AHEAD") : (Rel > 0.f ? TEXT("ON YOUR RIGHT") : TEXT("ON YOUR LEFT")));
	const FString Text = FString::Printf(TEXT("%s  %s  %.0f m"), Ghost->IsHuntWarning() ? TEXT("SHE'S COMING") : TEXT("SHE'S HERE"), Where, Dist);
	float TW = 0.f, TH = 0.f;
	GetTextSize(Text, TW, TH, Font, 1.1f);
	// Not holding the gun? Say how to fight, big and clear.
	if (const AGhostHunterCharacter* Hunter = Cast<AGhostHunterCharacter>(GetOwningPawn()))
	{
		const UGadgetComponent* G = Hunter->GetGadgets();
		if (G && G->GetEquipped() != EGadget::GunTorch && !Hunter->IsHiding())
		{
			const FString Fight = G->IsOwned(EGadget::GunTorch) ? TEXT("Press 2  -  then LEFT CLICK to fight!") : TEXT("RUN or hide in an almirah [E]!");
			float FW = 0.f, FH = 0.f;
			GetTextSize(Fight, FW, FH, Font, 1.6f);
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), CX - FW * 0.5f - 14.f, CY - Ring - 70.f, FW + 28.f, FH + 12.f);
			DrawText(Fight, FLinearColor(1.f, 0.85f, 0.4f, 1.f), CX - FW * 0.5f, CY - Ring - 64.f, Font, 1.6f);
		}
	}

	const float TY = FMath::Min(CY + Ring + 46.f, Canvas->ClipY - 150.f); // stay above the hotbar
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), CX - TW * 0.5f - 10.f, TY - 4.f, TW + 20.f, TH + 8.f);
	DrawText(Text, FLinearColor(1.f, 0.35f, 0.3f, 1.f), CX - TW * 0.5f, TY, Font, 1.1f);
}

void AGhostHuntHUD::DrawControlsPanel(UFont* Font)
{
	const AGhostHunterCharacter* Hunter = Cast<AGhostHunterCharacter>(GetOwningPawn());
	const UGadgetComponent* G = Hunter ? Hunter->GetGadgets() : nullptr;
	const float X = Canvas->ClipX - 300.f;
	if (!bControlsOpen || !Hunter || !G)
	{
		DrawText(TEXT("[H] Controls"), FLinearColor(1.f, 1.f, 1.f, 0.35f), Canvas->ClipX - 130.f, 64.f, Font, 0.8f);
		return;
	}

	struct FLine { FString Key; FString What; bool bHot; };
	TArray<FLine> Lines;
	const EGadget Eq = G->GetEquipped();

	TActorIterator<AGhostBase> GhostIt(GetWorld());
	const bool bHunt = GhostIt && GhostIt->IsHunting();

	if (Hunter->IsHiding())
	{
		Lines.Add({ TEXT("E"), TEXT("Get out of the almirah"), true });
		Lines.Add({ TEXT("Mouse"), TEXT("Look through the gaps"), false });
	}
	else
	{
		// What's in your hand comes first, highlighted.
		switch (Eq)
		{
		case EGadget::GunTorch:
			Lines.Add({ TEXT("Left click"), FString::Printf(TEXT("FIRE  (%d / %d shots)"), G->GetShotsLeft(), G->ShotsPerNight), true });
			Lines.Add({ TEXT(""), TEXT("Aim at her chest. 5 hits drive her off"), false });
			break;
		case EGadget::UVTorch:
			Lines.Add({ TEXT("F"), TEXT("UV light on / off"), true });
			Lines.Add({ TEXT(""), TEXT("Sweep walls near mirrors for marks"), false });
			break;
		case EGadget::Thermometer:
			Lines.Add({ TEXT("Read"), FString::Printf(TEXT("%.1f C  (colder = she's closer)"), G->GetTemperature()), true });
			break;
		case EGadget::EMFMeter:
			Lines.Add({ TEXT("Read"), FString::Printf(TEXT("EMF %d  (4-5 = she's near)"), G->GetEMFLevel()), true });
			break;
		default:
			Lines.Add({ TEXT("F"), TEXT("Torch on / off"), true });
			Lines.Add({ TEXT("Left click"), G->IsOwned(EGadget::GunTorch) ? TEXT("Press 2 first to fire") : TEXT("Fires only with Gun Torch (shop: B)"), false });
			break;
		}
		if (bHunt)
		{
			Lines.Add({ TEXT("Shift"), TEXT("RUN!  or hide: E at an almirah"), true });
		}
		const FText Prompt = Hunter->GetFocusedPrompt();
		Lines.Add({ TEXT("E"), Prompt.IsEmpty() ? FString(TEXT("Use / open / pick up")) : Prompt.ToString(), !Prompt.IsEmpty() });
		Lines.Add({ TEXT("W A S D"), TEXT("Move"), false });
		Lines.Add({ TEXT("Shift"), TEXT("Run"), false });
		Lines.Add({ TEXT("C"), TEXT("Crouch"), false });
		Lines.Add({ TEXT("1 - 5"), TEXT("Switch gadget"), false });
	}
	Lines.Add({ TEXT("Tab"), TEXT("Ghost Guide"), false });
	Lines.Add({ TEXT("M"), TEXT("Map"), false });
	Lines.Add({ TEXT("B"), TEXT("Shop"), false });
	Lines.Add({ TEXT("Esc"), TEXT("Menu / Exit"), false });
	Lines.Add({ TEXT("H"), TEXT("Hide this panel"), false });

	const float LineH = 20.f, W = 270.f;
	const float H = 30.f + Lines.Num() * LineH;
	const float Y = FMath::Max(64.f, Canvas->ClipY * 0.5f - H * 0.5f - 40.f);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), X, Y, W, H);
	DrawText(TEXT("CONTROLS"), Ink, X + 10.f, Y + 6.f, Font, 0.8f);

	float LY = Y + 28.f;
	for (const FLine& L : Lines)
	{
		if (!L.Key.IsEmpty())
		{
			float KW = 0.f, KH = 0.f;
			GetTextSize(L.Key, KW, KH, Font, 0.75f);
			DrawRect(L.bHot ? FLinearColor(0.55f, 0.18f, 0.06f, 0.95f) : FLinearColor(0.2f, 0.17f, 0.14f, 0.9f), X + 10.f, LY, KW + 10.f, 17.f);
			DrawText(L.Key, Paper, X + 15.f, LY + 1.f, Font, 0.75f);
		}
		DrawText(L.What, L.bHot ? FLinearColor(1.f, 0.85f, 0.5f, 1.f) : Faded, X + 98.f, LY + 1.f, Font, 0.75f);
		LY += LineH;
	}
}

void AGhostHuntHUD::DrawMap(UFont* Font)
{
	// Gather rooms in map space: u = east (-X), v = north (+Y).
	TArray<const ARoomArea*> Rooms;
	FBox2D Bounds(ForceInit);
	for (TActorIterator<ARoomArea> It(GetWorld()); It; ++It)
	{
		if (!It->bShowOnMap)
		{
			continue;
		}
		const FBox2D R = It->GetFloorRect();
		Bounds += FVector2D(-R.Max.X, R.Min.Y);
		Bounds += FVector2D(-R.Min.X, R.Max.Y);
		Rooms.Add(*It);
	}
	if (Rooms.Num() == 0 || !Bounds.bIsValid)
	{
		return;
	}

	const float Size = FMath::Min(Canvas->ClipX, Canvas->ClipY) * 0.72f;
	const float PX = (Canvas->ClipX - Size) * 0.5f, PY = (Canvas->ClipY - Size) * 0.5f + 10.f;
	const FVector2D Ext = Bounds.GetSize();
	const float Scale = Size / FMath::Max(Ext.X, Ext.Y);
	auto ToScreen = [&](float WorldX, float WorldY)
	{
		return FVector2D(PX + (-WorldX - Bounds.Min.X) * Scale, PY + (Bounds.Max.Y - WorldY) * Scale);
	};

	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.7f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	DrawRect(FLinearColor(0.09f, 0.07f, 0.05f, 0.95f), PX - 16.f, PY - 44.f, Size + 32.f, Size + 70.f);
	DrawText(TEXT("HAVELI  -  ground floor"), Ink, PX, PY - 34.f, Font, 1.05f);
	DrawText(TEXT("N ^"), FLinearColor(0.95f, 0.35f, 0.25f, 1.f), PX + Size - 30.f, PY - 34.f, Font, 1.f);

	for (const ARoomArea* Room : Rooms)
	{
		const FBox2D R = Room->GetFloorRect();
		const FVector2D A = ToScreen(R.Max.X, R.Max.Y); // top-left on screen (east is -X)
		const FVector2D B = ToScreen(R.Min.X, R.Min.Y); // bottom-right
		const bool bHere = Room == CurrentRoom.Get();
		const bool bGoal = bHasObjective && Room == ObjectiveRoom.Get();
		DrawRect(bHere ? FLinearColor(0.35f, 0.22f, 0.1f, 0.9f) : FLinearColor(0.18f, 0.14f, 0.1f, 0.9f), A.X + 1.f, A.Y + 1.f, B.X - A.X - 2.f, B.Y - A.Y - 2.f);
		const FLinearColor Edge = bGoal ? FLinearColor(1.f, 0.75f, 0.3f, 1.f) : FLinearColor(0.5f, 0.42f, 0.32f, 0.9f);
		const float T = bGoal ? 3.f : 1.f;
		DrawLine(A.X, A.Y, B.X, A.Y, Edge, T); DrawLine(B.X, A.Y, B.X, B.Y, Edge, T);
		DrawLine(B.X, B.Y, A.X, B.Y, Edge, T); DrawLine(A.X, B.Y, A.X, A.Y, Edge, T);

		const FString Name = Room->RoomName.ToString();
		float TW = 0.f, TH = 0.f;
		GetTextSize(Name, TW, TH, Font, 0.7f);
		if (TW < B.X - A.X - 4.f)
		{
			DrawText(Name, bHere ? Paper : Faded, (A.X + B.X - TW) * 0.5f, (A.Y + B.Y - TH) * 0.5f, Font, 0.7f);
		}
	}

	// You are here: an arrow pointing where you look.
	if (const APawn* Player = GetOwningPawn())
	{
		const FVector2D P = ToScreen(Player->GetActorLocation().X, Player->GetActorLocation().Y);
		const float Ang = FMath::DegreesToRadians(HeadingOf(GetOwningPlayerController()->GetControlRotation().Vector()));
		const FVector2D Fwd(FMath::Sin(Ang), -FMath::Cos(Ang));  // screen: right = east, up = north
		const FVector2D Side(-Fwd.Y, Fwd.X);
		const FVector2D Tip = P + Fwd * 12.f, L = P - Fwd * 7.f + Side * 7.f, R = P - Fwd * 7.f - Side * 7.f;
		const FLinearColor C(0.3f, 0.9f, 1.f, 1.f);
		DrawLine(Tip.X, Tip.Y, L.X, L.Y, C, 2.5f); DrawLine(L.X, L.Y, R.X, R.Y, C, 2.5f); DrawLine(R.X, R.Y, Tip.X, Tip.Y, C, 2.5f);
	}
	DrawText(TEXT("[M] close     gold outline = a room with signs still to find"), Faded, PX, PY + Size + 6.f, Font, 0.75f);
}

void AGhostHuntHUD::DrawTopBar(UFont* Font)
{
	const UGameInstance* GI = GetGameInstance();
	const UProgressSubsystem* Progress = GI ? GI->GetSubsystem<UProgressSubsystem>() : nullptr;
	const int64 Points = Progress ? Progress->GetPoints() : 0;

	// Shop button: cart-ish icon + points. Clickable whenever the mouse is visible.
	const float X = 32.f, Y = 14.f, W = 230.f, H = 30.f;
	const bool bHover = HoveredButton == TEXT("ShopIcon");
	DrawRect(bHover ? FLinearColor(0.45f, 0.08f, 0.06f, 0.9f) : FLinearColor(0.f, 0.f, 0.f, 0.45f), X, Y, W, H);
	// Little shop pictogram: a bag.
	DrawRect(Ink, X + 8.f, Y + 10.f, 16.f, 14.f);
	DrawLine(X + 11.f, Y + 10.f, X + 13.f, Y + 5.f, Ink, 2.f);
	DrawLine(X + 21.f, Y + 10.f, X + 19.f, Y + 5.f, Ink, 2.f);
	DrawLine(X + 13.f, Y + 5.f, X + 19.f, Y + 5.f, Ink, 2.f);
	DrawText(FString::Printf(TEXT("%s pts"), *FText::AsNumber(Points).ToString()), Paper, X + 32.f, Y + 6.f, Font, 0.95f);
	DrawText(TEXT("[B] Shop"), Faded, X + W - 64.f, Y + 7.f, Font, 0.8f);
	if (bMenuOpen || bShopOpen)
	{
		AddHitBox(FVector2D(X, Y), FVector2D(W, H), TEXT("ShopIcon"), true);
	}
}

void AGhostHuntHUD::DrawShop(UFont* Font)
{
	const UGameInstance* GI = GetGameInstance();
	const UProgressSubsystem* Progress = GI ? GI->GetSubsystem<UProgressSubsystem>() : nullptr;
	if (!Progress)
	{
		return;
	}

	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.75f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);

	const int32 N = GadgetTable::Num();
	const float CardW = 210.f, CardH = 300.f, Gap = 18.f;
	const float TotalW = N * CardW + (N - 1) * Gap;
	const float Scale = FMath::Min(1.f, (Canvas->ClipX - 60.f) / TotalW);
	const float CW = CardW * Scale, CH = CardH * Scale, G = Gap * Scale;
	const float StartX = (Canvas->ClipX - (N * CW + (N - 1) * G)) * 0.5f;
	const float TopY = Canvas->ClipY * 0.5f - CH * 0.5f;

	const FString Title = FString::Printf(TEXT("GADGET SHOP     %s points"), *FText::AsNumber(Progress->GetPoints()).ToString());
	float TW = 0.f, TH = 0.f;
	GetTextSize(Title, TW, TH, Font, 1.4f);
	DrawText(Title, Ink, (Canvas->ClipX - TW) * 0.5f, TopY - 70.f, Font, 1.4f);
	DrawText(TEXT("Click Buy. Press 1-5 in game to switch gadgets.   [B] or [Esc] to close"), Faded,
		StartX, TopY + CH + 18.f, Font, 0.85f);

	for (int32 i = 0; i < N; ++i)
	{
		const EGadget Gadget = static_cast<EGadget>(i);
		const FGadgetInfo& Info = GadgetTable::Info(Gadget);
		const bool bOwned = Progress->Owns(Gadget);
		const bool bAfford = Progress->GetPoints() >= Info.Price;
		const float X = StartX + i * (CW + G);

		DrawRect(FLinearColor(0.08f, 0.06f, 0.04f, 0.97f), X, TopY, CW, CH);
		DrawRect(FLinearColor(Info.Color.R, Info.Color.G, Info.Color.B, 0.6f), X, TopY, CW, 3.f);
		DrawText(FString::Printf(TEXT("%d"), i + 1), Faded, X + 10.f, TopY + 8.f, Font, 0.8f);

		DrawGadgetIcon(static_cast<uint8>(i), X + CW * 0.25f, TopY + 22.f * Scale, CW * 0.5f, Info.Color);
		DrawText(Info.Name, Paper, X + 12.f, TopY + CW * 0.5f + 36.f * Scale, Font, 1.05f);

		// Wrap the description by hand (Canvas text doesn't wrap).
		TArray<FString> Words;
		FString(Info.Description).ParseIntoArray(Words, TEXT(" "));
		FString Line;
		float Y = TopY + CW * 0.5f + 66.f * Scale;
		for (const FString& Word : Words)
		{
			const FString Try = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
			float LW = 0.f, LH = 0.f;
			GetTextSize(Try, LW, LH, Font, 0.75f);
			if (LW > CW - 24.f && !Line.IsEmpty())
			{
				DrawText(Line, Faded, X + 12.f, Y, Font, 0.75f);
				Y += 17.f;
				Line = Word;
			}
			else
			{
				Line = Try;
			}
		}
		if (!Line.IsEmpty())
		{
			DrawText(Line, Faded, X + 12.f, Y, Font, 0.75f);
		}

		// Price + button.
		const float BY = TopY + CH - 50.f;
		if (bOwned)
		{
			DrawText(Info.Price == 0 ? TEXT("Starter") : TEXT("Owned"), FLinearColor(0.5f, 0.9f, 0.5f, 1.f), X + 12.f, BY + 12.f, Font, 1.f);
		}
		else
		{
			DrawText(FString::Printf(TEXT("%s"), *FText::AsNumber(Info.Price).ToString()), bAfford ? Paper : FLinearColor(0.8f, 0.25f, 0.2f, 1.f),
				X + 12.f, BY + 12.f, Font, 1.f);
			if (bAfford)
			{
				DrawButton(Font, FName(*FString::Printf(TEXT("Buy_%d"), i)), TEXT("Buy"), X + CW - 92.f, BY + 4.f, 80.f, 36.f);
			}
		}
	}

	DrawButton(Font, TEXT("ShopClose"), TEXT("X"), StartX + N * CW + (N - 1) * G - 40.f, TopY - 76.f, 40.f, 36.f);
}

void AGhostHuntHUD::DrawButton(UFont* Font, FName Id, const FString& Label, float X, float Y, float W, float H)
{
	const bool bHover = HoveredButton == Id;
	DrawRect(bHover ? FLinearColor(0.45f, 0.08f, 0.06f, 0.95f) : FLinearColor(0.12f, 0.09f, 0.07f, 0.95f), X, Y, W, H);
	DrawRect(FLinearColor(0.85f, 0.65f, 0.35f, bHover ? 0.9f : 0.4f), X, Y + H - 2.f, W, 2.f);

	float TW = 0.f, TH = 0.f;
	GetTextSize(Label, TW, TH, Font, 1.1f);
	DrawText(Label, Paper, X + (W - TW) * 0.5f, Y + (H - TH) * 0.5f, Font, 1.1f);

	AddHitBox(FVector2D(X, Y), FVector2D(W, H), Id, /*bConsumesInput*/ true);
}

void AGhostHuntHUD::DrawMenu(UFont* Font)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.7f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);

	const float W = 320.f, H = 52.f, Gap = 16.f;
	const float X = (Canvas->ClipX - W) * 0.5f;

	// Exit sits on top, as asked.
	float Y = Canvas->ClipY * 0.28f;
	const FString Title = TEXT("THE LAST RITUAL");
	float TW = 0.f, TH = 0.f;
	GetTextSize(Title, TW, TH, Font, 1.6f);
	DrawText(Title, Ink, (Canvas->ClipX - TW) * 0.5f, Y - 70.f, Font, 1.6f);

	DrawButton(Font, TEXT("Exit"), TEXT("Exit game"), X, Y, W, H);
	Y += H + Gap;
	DrawButton(Font, TEXT("Resume"), TEXT("Resume"), X, Y, W, H);
	Y += H + Gap;
	DrawButton(Font, TEXT("Restart"), TEXT("Restart night"), X, Y, W, H);
	Y += H + Gap;
	{
		const URecordingSubsystem* Rec = GetGameInstance() ? GetGameInstance()->GetSubsystem<URecordingSubsystem>() : nullptr;
		DrawButton(Font, TEXT("Record"), Rec && Rec->IsRecording() ? TEXT("Stop recording  [F9]") : TEXT("Record gameplay  [F9]"), X, Y, W, H);
	}

	DrawText(TEXT("Esc to resume"), Faded, X, Y + H + 24.f, Font, 0.8f);
}

void AGhostHuntHUD::NotifyHitBoxClick(FName BoxName)
{
	Super::NotifyHitBoxClick(BoxName);
	AGhostHunterCharacter* ClickHunter = Cast<AGhostHunterCharacter>(GetOwningPawn());
	if (ClickHunter)
	{
		ClickHunter->PlayUiSound();
	}

	if (BoxName == TEXT("ShopIcon"))
	{
		SetShopOpen(true);
		return;
	}
	if (BoxName == TEXT("ShopClose"))
	{
		SetShopOpen(false);
		return;
	}
	const FString Box = BoxName.ToString();
	if (Box.StartsWith(TEXT("Buy_")))
	{
		const int32 Index = FCString::Atoi(*Box.RightChop(4));
		UGameInstance* GI = GetGameInstance();
		UProgressSubsystem* Progress = GI ? GI->GetSubsystem<UProgressSubsystem>() : nullptr;
		if (Progress && Index >= 0 && Index < GadgetTable::Num() && Progress->Buy(static_cast<EGadget>(Index)))
		{
			if (ClickHunter)
			{
				ClickHunter->PlayUiSound(/*bPurchase*/ true);
			}
			ShowMessage(FText::Format(LOCTEXT("Bought", "Bought the {0}. Press {1} to use it."),
				FText::FromString(GadgetTable::Info(static_cast<EGadget>(Index)).Name), FText::AsNumber(Index + 1)), 3.f);
		}
		return;
	}

	if (BoxName == TEXT("Resume"))
	{
		SetMenuOpen(false);
	}
	else if (BoxName == TEXT("Restart"))
	{
		SetMenuOpen(false);
		if (UNightSubsystem* Night = GetWorld()->GetSubsystem<UNightSubsystem>())
		{
			Night->RestartNight();
		}
	}
	else if (BoxName == TEXT("Record"))
	{
		if (URecordingSubsystem* Rec = GetGameInstance() ? GetGameInstance()->GetSubsystem<URecordingSubsystem>() : nullptr)
		{
			const bool bWasRecording = Rec->IsRecording();
			const FText Msg = Rec->Toggle();
			if (!bWasRecording && Rec->IsRecording())
			{
				SetMenuOpen(false); // get the menu out of the shot
			}
			ShowMessage(Msg, 6.f);
		}
	}
	else if (BoxName == TEXT("Exit"))
	{
		UKismetSystemLibrary::QuitGame(this, GetOwningPlayerController(), EQuitPreference::Quit, false);
	}
}

void AGhostHuntHUD::NotifyHitBoxBeginCursorOver(FName BoxName)
{
	Super::NotifyHitBoxBeginCursorOver(BoxName);
	HoveredButton = BoxName;
}

void AGhostHuntHUD::NotifyHitBoxEndCursorOver(FName BoxName)
{
	Super::NotifyHitBoxEndCursorOver(BoxName);
	if (HoveredButton == BoxName)
	{
		HoveredButton = NAME_None;
	}
}

void AGhostHuntHUD::DrawHuntVignette()
{
	TActorIterator<AGhostBase> It(GetWorld());
	const AGhostBase* Ghost = It ? *It : nullptr;
	if (!Ghost || !Ghost->IsHunting())
	{
		return;
	}

	// Pulsing dark-red edges, stronger when she's close. During the warning they creep in.
	float Closeness = 0.f;
	if (Ghost->IsHuntWarning())
	{
		Closeness = 0.35f * Ghost->GetHuntWarningProgress();
	}
	else if (const APawn* Player = GetOwningPawn())
	{
		Closeness = FMath::GetMappedRangeValueClamped(FVector2D(1500.f, 200.f), FVector2D(0.f, 1.f),
			FVector::Dist2D(Ghost->GetActorLocation(), Player->GetActorLocation()));
	}
	const float Pulse = 0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * (3.f + 6.f * Closeness));
	const float Alpha = (0.08f + 0.3f * Closeness) * (0.6f + 0.4f * Pulse);

	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const int32 Bands = 6;
	for (int32 i = 0; i < Bands; ++i)
	{
		const float T = 18.f * (i + 1);
		const FLinearColor C(0.35f, 0.f, 0.f, Alpha / Bands);
		DrawRect(C, 0.f, 0.f, W, T);
		DrawRect(C, 0.f, H - T, W, T);
		DrawRect(C, 0.f, 0.f, T, H);
		DrawRect(C, W - T, 0.f, T, H);
	}
}

void AGhostHuntHUD::DrawHidingSlats(UFont* Font)
{
	const AGhostHunterCharacter* Hunter = Cast<AGhostHunterCharacter>(GetOwningPawn());
	if (!Hunter || !Hunter->IsHiding())
	{
		return;
	}

	// Looking out through the gaps in the almirah doors.
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const float Slat = W * 0.075f, Gap = W * 0.018f;
	for (float X = -Gap * 0.5f; X < W; X += Slat + Gap)
	{
		DrawRect(FLinearColor(0.02f, 0.015f, 0.01f, 0.93f), X, 0.f, Slat, H);
	}
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), 0.f, 0.f, W, H * 0.12f);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), 0.f, H * 0.88f, W, H * 0.12f);
}

void AGhostHuntHUD::DrawObjectives(UFont* Font)
{
	const UNightSubsystem* Night = GetWorld()->GetSubsystem<UNightSubsystem>();
	const UGhostGuideSubsystem* Guide = GetWorld()->GetSubsystem<UGhostGuideSubsystem>();
	if (!Night || !Night->IsPlaying() || !Guide)
	{
		return;
	}

	// Generic steps only: never name the ghost or its ritual (the player works that out).
	const int32 Clues = Guide->GetFoundCluesRef().Num();
	const int32 Step = Clues < 3 ? 0 : 1;
	const TCHAR* Steps[] = {
		TEXT("Search the house for signs"),
		TEXT("Work out which spirit this is  [Tab]"),
		TEXT("Perform its ritual"),
	};

	float Y = 80.f;
	for (int32 i = 0; i < 3; ++i)
	{
		const bool bDone = i < Step;
		const bool bCurrent = i == Step || (Step == 1 && i == 2);
		const FString Line = FString::Printf(TEXT("%s %s%s"), bDone ? TEXT("[x]") : (bCurrent ? TEXT("[>]") : TEXT("[ ]")), Steps[i],
			i == 0 ? *FString::Printf(TEXT("  (%d found)"), Clues) : TEXT(""));
		DrawText(Line, bCurrent ? Paper : Faded, 40.f, Y, Font, 0.85f);
		Y += 22.f;
	}
}

void AGhostHuntHUD::DrawLives(UFont* Font)
{
	const UNightSubsystem* Night = GetWorld()->GetSubsystem<UNightSubsystem>();
	if (!Night || Night->GetState() == ENightState::NotStarted)
	{
		return;
	}

	const float Size = 14.f, Gap = 8.f;
	const int32 Max = Night->GetMaxLives();
	const float StartX = Canvas->ClipX - 40.f - Max * (Size + Gap);
	const float Y = 40.f;

	DrawText(TEXT("LIVES"), FLinearColor(1.f, 1.f, 1.f, 0.5f), StartX, Y - 22.f, Font, 0.8f);
	for (int32 i = 0; i < Max; ++i)
	{
		const bool bAlive = i < Night->GetLives();
		DrawRect(bAlive ? FLinearColor(0.7f, 0.05f, 0.05f, 0.9f) : FLinearColor(0.2f, 0.2f, 0.2f, 0.5f),
			StartX + i * (Size + Gap), Y, Size, Size);
	}
}

void AGhostHuntHUD::DrawNightResult(UFont* Font)
{
	const UNightSubsystem* Night = GetWorld()->GetSubsystem<UNightSubsystem>();
	if (!Night || (Night->GetState() != ENightState::Won && Night->GetState() != ENightState::Lost))
	{
		return;
	}

	const bool bWon = Night->GetState() == ENightState::Won;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.75f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);

	FString Title;
	FString Sub;
	if (bWon)
	{
		const FString Name = Night->GetTonightGhost() ? Night->GetTonightGhost()->DisplayName.ToString() : TEXT("The spirit");
		Title = FString::Printf(TEXT("%s is at peace."), *Name);
		Sub = TEXT("The house is a little quieter tonight.");
	}
	else
	{
		Title = TEXT("You did not survive the night.");
		Sub = TEXT("The night begins again...");
	}

	float W = 0.f, H = 0.f;
	GetTextSize(Title, W, H, Font, 2.f);
	DrawText(Title, bWon ? Paper : FLinearColor(0.8f, 0.1f, 0.1f, 1.f), (Canvas->ClipX - W) * 0.5f, Canvas->ClipY * 0.42f, Font, 2.f);
	GetTextSize(Sub, W, H, Font);
	DrawText(Sub, Faded, (Canvas->ClipX - W) * 0.5f, Canvas->ClipY * 0.42f + 60.f, Font);
}

void AGhostHuntHUD::HandleClueRecorded(const FFoundClue& Clue)
{
	ShowMessage(FText::Format(LOCTEXT("ClueAdded", "Added to Ghost Guide: {0}"), Clue.DisplayName));
}

void AGhostHuntHUD::ShowMessage(const FText& Message, float Seconds)
{
	MessageText = Message;
	MessageHideTime = GetWorld()->GetTimeSeconds() + Seconds;
}

void AGhostHuntHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;

	NavRefreshTimer -= GetWorld()->GetDeltaSeconds();
	if (NavRefreshTimer <= 0.f)
	{
		NavRefreshTimer = 0.4f;
		UpdateNavTargets();
	}

	if (bGuideOpen)
	{
		DrawGhostGuide(Font);
	}
	else
	{
		DrawHuntVignette();
		DrawHidingSlats(Font);
		DrawGameplay(Font);
		DrawLives(Font);
		DrawObjectives(Font);
		if (!bShopOpen && !bMenuOpen && !bMapOpen)
		{
			DrawFloorPath();
			DrawWaypoint(Font);
			DrawGhostDirection(Font);
			DrawControlsPanel(Font);
		}
		DrawGhostHealth(Font);
		DrawGadgetReadout(Font);
		DrawHotbar(Font);
	}

	DrawNightResult(Font);

	if (bShopOpen)
	{
		DrawShop(Font);
	}
	else if (bMenuOpen)
	{
		DrawMenu(Font);
	}
	else
	{
		DrawText(TEXT("[Esc] Menu"), FLinearColor(1.f, 1.f, 1.f, 0.35f), 36.f, 50.f, Font, 0.8f);
	}
	// Recording indicator.
	if (const URecordingSubsystem* Rec = GetGameInstance() ? GetGameInstance()->GetSubsystem<URecordingSubsystem>() : nullptr)
	{
		if (Rec->IsRecording())
		{
			const int32 T = FMath::FloorToInt(Rec->GetElapsed());
			const bool bBlink = (FMath::FloorToInt(Rec->GetElapsed() * 2.0) % 2) == 0;
			const float RX = Canvas->ClipX - 300.f, RY = 14.f;
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), RX, RY, 104.f, 26.f);
			if (bBlink)
			{
				DrawRect(FLinearColor(0.9f, 0.1f, 0.08f, 1.f), RX + 8.f, RY + 8.f, 10.f, 10.f);
			}
			DrawText(FString::Printf(TEXT("REC %02d:%02d"), T / 60, T % 60), FLinearColor(1.f, 0.85f, 0.85f, 1.f), RX + 26.f, RY + 4.f, Font, 0.9f);
		}
	}

	if (!bGuideOpen)
	{
		DrawTopBar(Font);
		if (!bShopOpen && !bMenuOpen)
		{
			DrawCompass(Font);
		}
	}
	if (bMapOpen && !bGuideOpen && !bShopOpen && !bMenuOpen)
	{
		DrawMap(Font);
	}

	// Message at the top, shown over both views.
	if (!MessageText.IsEmpty() && GetWorld()->GetTimeSeconds() < MessageHideTime)
	{
		const FString Text = MessageText.ToString();
		float W = 0.f, H = 0.f;
		GetTextSize(Text, W, H, Font);
		DrawText(Text, Paper, Canvas->ClipX * 0.5f - W * 0.5f, 60.f, Font);
	}
}

void AGhostHuntHUD::DrawGameplay(UFont* Font)
{
	const AGhostHunterCharacter* Hunter = Cast<AGhostHunterCharacter>(GetOwningPawn());
	if (!Hunter)
	{
		return;
	}

	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;

	// Crosshair: a tiny dot, bigger when looking at something usable.
	const bool bHasFocus = Hunter->GetFocusedActor() != nullptr;
	const float Dot = bHasFocus ? 6.f : 3.f;
	DrawRect(FLinearColor(1.f, 1.f, 1.f, bHasFocus ? 0.9f : 0.4f), CX - Dot * 0.5f, CY - Dot * 0.5f, Dot, Dot);

	// Interaction prompt under the crosshair.
	const FText Prompt = Hunter->GetFocusedPrompt();
	if (!Prompt.IsEmpty())
	{
		const FString Text = FString::Printf(TEXT("[E]  %s"), *Prompt.ToString());
		float TextW = 0.f, TextH = 0.f;
		GetTextSize(Text, TextW, TextH, Font);
		DrawText(Text, Paper, CX - TextW * 0.5f, CY + 28.f, Font);
	}

	// Torch battery bar, bottom-left.
	if (const UTorchComponent* Torch = Hunter->GetTorch())
	{
		const float Pct = Torch->GetBatteryPercent();
		const float BarW = 180.f, BarH = 8.f;
		const float X = 40.f, Y = Canvas->ClipY - 50.f;

		const FLinearColor Fill = Torch->IsBatteryLow()
			? FLinearColor(0.85f, 0.15f, 0.1f, 0.9f)
			: FLinearColor(0.95f, 0.85f, 0.6f, 0.8f);

		DrawText(TEXT("TORCH"), FLinearColor(1.f, 1.f, 1.f, 0.5f), X, Y - 22.f, Font, 0.8f);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), X, Y, BarW, BarH);
		DrawRect(Fill, X, Y, BarW * Pct, BarH);
	}

}

void AGhostHuntHUD::DrawGhostGuide(UFont* Font)
{
	const UGhostGuideSubsystem* Guide = GetWorld()->GetSubsystem<UGhostGuideSubsystem>();

	// Notebook page.
	const float PageW = FMath::Min(1100.f, Canvas->ClipX - 80.f);
	const float PageH = FMath::Min(650.f, Canvas->ClipY - 80.f);
	const float PX = (Canvas->ClipX - PageW) * 0.5f;
	const float PY = (Canvas->ClipY - PageH) * 0.5f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	DrawRect(FLinearColor(0.08f, 0.06f, 0.04f, 0.97f), PX, PY, PageW, PageH);

	const float Pad = 30.f;
	const float LineH = 26.f;
	const float ColW = (PageW - Pad * 3.f) * 0.5f;
	const float LeftX = PX + Pad;
	const float RightX = LeftX + ColW + Pad;

	DrawText(TEXT("GHOST GUIDE"), Ink, LeftX, PY + Pad, Font, 1.3f);
	DrawText(TEXT("Some signs lie. Trust the pattern, not one clue."), Faded, LeftX, PY + Pad + 34.f, Font, 0.85f);

	const float TopY = PY + Pad + 80.f;

	// Left: clues found tonight.
	DrawText(TEXT("Clues found"), Ink, LeftX, TopY, Font);
	float Y = TopY + LineH * 1.5f;
	if (!Guide || Guide->GetFoundCluesRef().Num() == 0)
	{
		DrawText(TEXT("Nothing yet. Search the house."), Faded, LeftX, Y, Font, 0.9f);
	}
	else
	{
		for (const FFoundClue& Clue : Guide->GetFoundCluesRef())
		{
			DrawText(FString::Printf(TEXT("- %s"), *Clue.DisplayName.ToString()), Paper, LeftX, Y, Font, 0.9f);
			Y += LineH;
			if (!Clue.Description.IsEmpty())
			{
				DrawText(Clue.Description.ToString(), Faded, LeftX + 16.f, Y, Font, 0.75f);
				Y += LineH;
			}
		}
	}

	// Right: ghosts and how many of their signs were found.
	DrawText(TEXT("Ghosts"), Ink, RightX, TopY, Font);
	Y = TopY + LineH * 1.5f;
	if (!Guide || Guide->GetGhosts().Num() == 0)
	{
		DrawText(TEXT("No ghost definitions found in /Game/Ghosts."), Faded, RightX, Y, Font, 0.9f);
		return;
	}

	for (const FGhostMatch& Match : Guide->GetGhostMatches())
	{
		if (!Match.Ghost)
		{
			continue;
		}

		const FLinearColor Color = Match.SignsFound > 0 ? Paper : Faded;
		DrawText(FString::Printf(TEXT("%s   %d / %d"), *Match.Ghost->DisplayName.ToString(), Match.SignsFound, Match.SignsTotal),
			Color, RightX, Y, Font, 0.95f);
		Y += LineH;

		// List signs, ticking the ones found.
		for (const FGhostSign& Sign : Match.Ghost->Signs)
		{
			const bool bFound = Guide->HasFoundSign(Sign.SignId);
			DrawText(FString::Printf(TEXT("%s %s"), bFound ? TEXT("[x]") : TEXT("[ ]"), *Sign.Description.ToString()),
				bFound ? Paper : Faded, RightX + 16.f, Y, Font, 0.75f);
			Y += LineH * 0.85f;
		}
		Y += LineH * 0.4f;
	}
}

#undef LOCTEXT_NAMESPACE
