#include "RecordingSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "Widgets/SWindow.h"

#if PLATFORM_LINUX
#include <signal.h>
#endif

#define LOCTEXT_NAMESPACE "GhostHunt"

void URecordingSubsystem::Deinitialize()
{
	if (bRecording)
	{
		Stop(); // never leave ffmpeg running after the game closes
	}
	Super::Deinitialize();
}

FText URecordingSubsystem::Toggle()
{
	return bRecording ? Stop() : Start();
}

double URecordingSubsystem::GetElapsed() const
{
	return bRecording ? FPlatformTime::Seconds() - StartTime : 0.0;
}

FString URecordingSubsystem::FindFfmpeg() const
{
	for (const TCHAR* Candidate : { TEXT("/usr/bin/ffmpeg"), TEXT("/usr/local/bin/ffmpeg"), TEXT("/snap/bin/ffmpeg") })
	{
		if (FPaths::FileExists(Candidate))
		{
			return Candidate;
		}
	}
	return FString();
}

FText URecordingSubsystem::Start()
{
	if (bRecording)
	{
		return LOCTEXT("RecAlready", "Already recording.");
	}

#if PLATFORM_LINUX
	const FString Ffmpeg = FindFfmpeg();
	if (Ffmpeg.IsEmpty())
	{
		return LOCTEXT("RecNoFfmpeg", "Recording needs ffmpeg. Install it with: sudo apt install ffmpeg");
	}

	// The game window's position and size on the desktop.
	TSharedPtr<SWindow> Window = (GEngine && GEngine->GameViewport) ? GEngine->GameViewport->GetWindow() : nullptr;
	if (!Window.IsValid())
	{
		return LOCTEXT("RecNoWindow", "Couldn't find the game window to record.");
	}
	const FVector2D Pos = Window->GetPositionInScreen();
	const FVector2D Size = Window->GetSizeInScreen();
	// x264 needs even dimensions.
	const int32 W = FMath::Max(2, FMath::RoundToInt(Size.X) & ~1);
	const int32 H = FMath::Max(2, FMath::RoundToInt(Size.Y) & ~1);

	FString Display = FPlatformMisc::GetEnvironmentVariable(TEXT("DISPLAY"));
	if (Display.IsEmpty())
	{
		Display = TEXT(":0");
	}

	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Recordings"));
	IFileManager::Get().MakeDirectory(*Dir, true);
	OutputFile = Dir / FString::Printf(TEXT("TheLastRitual_%s.mp4"), *FDateTime::Now().ToString(TEXT("%Y-%m-%d_%H-%M-%S")));

	// Picture: the window region at 30 fps. Sound: whatever the default output is playing (the game).
	const FString Args = FString::Printf(
		TEXT("-hide_banner -loglevel error -y ")
		TEXT("-f x11grab -draw_mouse 0 -framerate 30 -video_size %dx%d -i %s+%d,%d ")
		TEXT("-f pulse -i @DEFAULT_MONITOR@ ")
		TEXT("-c:v libx264 -preset veryfast -crf 22 -pix_fmt yuv420p ")
		TEXT("-c:a aac -b:a 160k -movflags +faststart \"%s\""),
		W, H, *Display, FMath::RoundToInt(Pos.X), FMath::RoundToInt(Pos.Y), *OutputFile);

	Proc = FPlatformProcess::CreateProc(*Ffmpeg, *Args, /*bLaunchDetached*/ false, /*bLaunchHidden*/ true, /*bReallyHidden*/ true,
		&ProcId, /*PriorityModifier*/ 0, nullptr, nullptr);
	if (!Proc.IsValid())
	{
		return LOCTEXT("RecFailed", "Couldn't start ffmpeg.");
	}

	bRecording = true;
	StartTime = FPlatformTime::Seconds();
	UE_LOG(LogTemp, Log, TEXT("Recording started (pid %u): ffmpeg %s"), ProcId, *Args);
	return LOCTEXT("RecStarted", "Recording gameplay with sound. Press F9 or use the Esc menu to stop.");
#else
	return LOCTEXT("RecLinuxOnly", "Recording is only built for Linux so far.");
#endif
}

FText URecordingSubsystem::Stop()
{
	if (!bRecording)
	{
		return LOCTEXT("RecNotRunning", "Not recording.");
	}
	bRecording = false;

#if PLATFORM_LINUX
	// SIGINT lets ffmpeg finish the MP4 properly (a killed ffmpeg leaves an unplayable file).
	if (ProcId != 0)
	{
		::kill(static_cast<pid_t>(ProcId), SIGINT);
	}
	const double Deadline = FPlatformTime::Seconds() + 8.0;
	while (FPlatformProcess::IsProcRunning(Proc) && FPlatformTime::Seconds() < Deadline)
	{
		FPlatformProcess::Sleep(0.05f);
	}
	if (FPlatformProcess::IsProcRunning(Proc))
	{
		FPlatformProcess::TerminateProc(Proc, true);
	}
#endif
	FPlatformProcess::CloseProc(Proc);
	ProcId = 0;

	const bool bSaved = FPaths::FileExists(OutputFile) && IFileManager::Get().FileSize(*OutputFile) > 0;
	UE_LOG(LogTemp, Log, TEXT("Recording stopped: %s (%s)"), *OutputFile, bSaved ? TEXT("saved") : TEXT("FAILED"));
	return bSaved
		? FText::Format(LOCTEXT("RecSaved", "Recording saved: {0}"), FText::FromString(OutputFile))
		: LOCTEXT("RecNoFile", "Recording failed: no video file was written.");
}

#undef LOCTEXT_NAMESPACE
