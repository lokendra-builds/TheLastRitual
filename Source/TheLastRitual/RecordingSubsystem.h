#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RecordingSubsystem.generated.h"

/**
 * Records gameplay (the game window + the game's sound) to an MP4 in
 * Saved/Recordings/, using ffmpeg. Linux: x11grab for the picture and the
 * PulseAudio/PipeWire monitor of the default output for the sound.
 *
 * Started and stopped from the Esc menu ("Record gameplay") or F9.
 */
UCLASS()
class THELASTRITUAL_API URecordingSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** Start or stop. Returns a message for the player. */
	FText Toggle();

	FText Start();
	FText Stop();

	bool IsRecording() const { return bRecording; }

	/** Seconds since recording started. */
	double GetElapsed() const;

	/** Where the last file was written. */
	const FString& GetLastFile() const { return OutputFile; }

private:
	FString FindFfmpeg() const;

	FProcHandle Proc;
	uint32 ProcId = 0;
	bool bRecording = false;
	double StartTime = 0.0;
	FString OutputFile;
};
