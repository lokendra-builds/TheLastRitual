// The glowing path on the floor from the player to the objective room.
// Part of AGhostHuntHUD; kept in its own file.

#include "GhostHuntHUD.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "RoomArea.h"

namespace
{
	constexpr float MarkSpacing = 70.f;     // cm between marks
	constexpr float MaxPathLength = 1600.f; // only show the next 16 m
	constexpr float FirstMarkOffset = 160.f; // start a little ahead of your feet (closer marks look huge)
}

void AGhostHuntHUD::UpdateFloorPath()
{
	FloorPath.Reset();

	const APawn* Player = GetOwningPawn();
	if (!Player || !bHasObjective)
	{
		return;
	}

	const FVector From = Player->GetActorLocation() - FVector(0.f, 0.f, 88.f);
	FVector To(ObjectiveLocation.X, ObjectiveLocation.Y, From.Z);

	// The room's centre can be inside furniture (the courtyard centre is the well):
	// aim for the nearest walkable spot instead.
	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation OnNav;
		if (Nav->ProjectPointToNavigation(To, OnNav, FVector(400.f, 400.f, 300.f)))
		{
			To = OnNav.Location;
		}
	}
	UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), From, To);
	if (!Path || !Path->IsValid() || Path->PathPoints.Num() < 2)
	{
		return;
	}

	// Walk the polyline and drop a mark every MarkSpacing.
	float Travelled = 0.f;
	float NextMark = FirstMarkOffset;
	for (int32 i = 1; i < Path->PathPoints.Num() && Travelled < MaxPathLength; ++i)
	{
		const FVector A = Path->PathPoints[i - 1];
		const FVector B = Path->PathPoints[i];
		const float Seg = FVector::Dist(A, B);
		while (NextMark <= Travelled + Seg && NextMark < MaxPathLength)
		{
			const float T = (NextMark - Travelled) / FMath::Max(Seg, 1.f);
			FloorPath.Add(FMath::Lerp(A, B, T));
			NextMark += MarkSpacing;
		}
		Travelled += Seg;
	}
}

void AGhostHuntHUD::DrawFloorPath()
{
	FloorPathTimer -= GetWorld()->GetDeltaSeconds();
	if (FloorPathTimer <= 0.f)
	{
		FloorPathTimer = 0.35f;
		UpdateFloorPath();
	}

	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !PC->PlayerCameraManager || FloorPath.Num() < 2)
	{
		return;
	}

	const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
	const FVector ViewDir = PC->PlayerCameraManager->GetCameraRotation().Vector();

	FVector2D ViewportSize(1.f, 1.f);
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}
	const FVector2D ToCanvas(Canvas->ClipX / FMath::Max(ViewportSize.X, 1.f), Canvas->ClipY / FMath::Max(ViewportSize.Y, 1.f));

	auto Project = [&](const FVector& World, FVector2D& Out) -> bool
	{
		if (FVector::DotProduct(ViewDir, (World - Cam).GetSafeNormal()) < 0.15f)
		{
			return false;
		}
		if (!PC->ProjectWorldLocationToScreen(World, Out, true))
		{
			return false;
		}
		Out *= ToCanvas;
		return true;
	};

	const float Time = GetWorld()->GetTimeSeconds();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FloorPath), false, GetOwningPawn());

	for (int32 i = 0; i < FloorPath.Num(); ++i)
	{
		const FVector P = FloorPath[i] + FVector(0.f, 0.f, 3.f);
		const FVector Next = (i + 1 < FloorPath.Num()) ? FloorPath[i + 1] : P + (P - FloorPath[i - 1]);
		FVector Dir = (Next - P);
		Dir.Z = 0.f;
		Dir = Dir.GetSafeNormal();
		if (Dir.IsNearlyZero())
		{
			continue;
		}
		const FVector Side(-Dir.Y, Dir.X, 0.f);

		// Hidden behind a wall? Then don't draw it.
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Cam, P + FVector(0.f, 0.f, 5.f), ECC_Visibility, Params))
		{
			continue;
		}

		// A chevron lying flat on the floor, pointing along the path.
		const float Size = 15.f;
		const FVector Tip = P + Dir * Size;
		const FVector L = P - Dir * Size * 0.4f + Side * Size;
		const FVector R = P - Dir * Size * 0.4f - Side * Size;
		FVector2D STip, SL, SR;
		if (!Project(Tip, STip) || !Project(L, SL) || !Project(R, SR))
		{
			continue;
		}

		// Fade with distance and pulse along the path, like a trail of diya light.
		const float Dist = FVector::Dist(P, Cam);
		const float Fade = FMath::Clamp(1.f - (Dist - 200.f) / 1400.f, 0.15f, 1.f);
		const float Wave = 0.55f + 0.45f * FMath::Sin(Time * 5.f - i * 0.6f);
		const FLinearColor C(1.f, 0.72f, 0.3f, Fade * Wave);
		const float Thick = FMath::Clamp(3.5f * (400.f / FMath::Max(Dist, 100.f)), 1.5f, 4.f);
		DrawLine(SL.X, SL.Y, STip.X, STip.Y, C, Thick);
		DrawLine(SR.X, SR.Y, STip.X, STip.Y, C, Thick);
	}
}
