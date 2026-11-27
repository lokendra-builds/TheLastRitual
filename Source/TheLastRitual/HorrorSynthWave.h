#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundWaveProcedural.h"
#include "Containers/Queue.h"
#include <atomic>
#include "HorrorSynthWave.generated.h"

/** One-shot sound effects, all synthesised (see UHorrorSynthWave::PlaySfx). */
enum class EHorrorSfx : uint8
{
	Footstep,       // low gritty thud on old stone, random pitch / brightness
	DoorOpen,       // slow wobbling wooden creak
	DoorClose,      // short creak + thud + latch
	DoorLocked,     // handle / latch rattle
	DoorSlam,       // loud boom + rattle + tail
	Pickup,         // cloth / paper rustle + small chime
	ClueFound,      // soft low ghanta bell
	MirrorCover,    // cloth swish
	AlmirahIn,      // wooden creak, then the door thumps shut
	AlmirahOut,     // latch, creak, step out
	TorchClick,     // switch press + release
	BatteryPickup,  // plastic clatter + snap + blip
	RitualCorrect,  // bright temple bell + rising Sa-Ga-Pa-Sa' chord
	RitualWrong,    // low dissonant hit (layered with the sting)
	UIClick,        // soft menu click
	Purchase,       // coins + chime
	Count
};

/**
 * All of the game's sound, synthesised in code (no audio files):
 *
 *   ambience  low drone + wind that rises and falls
 *   heartbeat "lub-dub" thumps; rate and volume set from gameplay
 *   whisper   breathy band-passed noise in syllable-like bursts (ghost nearby)
 *   sting     dissonant hit + low boom (hunt begins / jump scare)
 *   music     tanpura drone (Sa / Pa, Karplus-Strong strings with a jawari buzz)
 *             and a sparse bansuri-like melody in raga Bhairav, with a soft echo
 *   sfx       pool of one-shot voices (footsteps, doors, pickups, bells, UI...)
 *
 * Gameplay sets the levels from the game thread; the audio thread reads them.
 * One-shots go through a lock-free MPSC queue.
 */
UCLASS()
class THELASTRITUAL_API UHorrorSynthWave : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	UHorrorSynthWave(const FObjectInitializer& ObjectInitializer);

	virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) override;

	// 0..1 levels. Safe to call from the game thread.
	void SetAmbienceLevel(float V) { AmbienceTarget.store(FMath::Clamp(V, 0.f, 1.f)); }
	void SetHeartbeat(float Bpm, float Level) { HeartBpm.store(FMath::Clamp(Bpm, 30.f, 200.f)); HeartTarget.store(FMath::Clamp(Level, 0.f, 1.f)); }
	void SetWhisperLevel(float V) { WhisperTarget.store(FMath::Clamp(V, 0.f, 1.f)); }
	void TriggerSting(float Strength = 1.f) { StingStrength.store(Strength); StingRequests.fetch_add(1); }
	/** EMF meter pips: Rate beeps per second (0 = off). */
	void SetBeep(float BeepsPerSecond, float Level) { BeepRate.store(FMath::Max(0.f, BeepsPerSecond)); BeepTarget.store(FMath::Clamp(Level, 0.f, 1.f)); }
	/** Gun Torch shot. */
	void TriggerZap() { ZapRequests.fetch_add(1); }
	/** Background music level 0..1 (fades over ~2 s). */
	void SetMusicLevel(float V) { MusicTarget.store(FMath::Clamp(V, 0.f, 1.f)); }

	/** Play a one-shot effect. Safe from any thread. */
	void PlaySfx(EHorrorSfx Sfx, float Volume = 1.f);

	/** Effects only (no ambience / music / heartbeat). Used for the UI synth that keeps playing while paused. */
	void SetSfxOnly(bool bOnly) { bSfxOnly = bOnly; }

	// ---- Helpers for gameplay code (all null-safe).

	/** The local player's synth, or null. */
	static UHorrorSynthWave* Get(const UObject* WorldContext);
	/** Play on the player's synth. UIClick / Purchase go to the pause-proof UI synth. */
	static void Play(const UObject* WorldContext, EHorrorSfx Sfx, float Volume = 1.f);
	/** Play with simple distance attenuation from the player (2D synth, so no panning). */
	static void PlayAt(const UObject* WorldContext, EHorrorSfx Sfx, const FVector& Where, float Volume = 1.f, float MinVolume = 0.f);

	/** Offline test: renders footsteps, every effect, music and a full mix to a WAV (+ a CSV of segments). */
	static void RenderTestWav(const FString& WavPath, float MusicSeconds);

private:
	static constexpr int32 Rate = 48000;
	static constexpr int32 MaxVoices = 16;
	static constexpr int32 KsStrings = 4;
	static constexpr int32 KsMax = 1024;
	static constexpr int32 EchoLen = 20640; // 0.43 s

	struct FSfxRequest
	{
		EHorrorSfx Sfx = EHorrorSfx::UIClick;
		float Volume = 1.f;
	};

	struct FVoice
	{
		EHorrorSfx Type = EHorrorSfx::UIClick;
		bool bActive = false;
		float T = 0.f, Len = 0.f, Vol = 1.f, Pitch = 1.f, Bright = 0.5f;
		float Ph[6] = {};
		float F[8] = {};
		float R[4] = {};
	};

	float Noise();
	float Rand01() { return 0.5f + 0.5f * Noise(); }

	float RenderBed(float N, float Dt, float BeatPeriod, float Smooth, float BeepR, float BeepLevelT);
	float RenderMusic(float Dt);
	float RenderVoice(FVoice& V, float Dt);
	void StartVoice(EHorrorSfx Sfx, float Volume);
	void PluckString(int32 Index, float Strength);
	void StartFluteNote(bool bFirst, bool bLast);

	float Creak(FVoice& V, float t, float Dur, float BaseRate, float Res1, float Res2, float Dt);

	bool bSfxOnly = false;

	std::atomic<float> AmbienceTarget{ 0.6f };
	std::atomic<float> HeartBpm{ 70.f };
	std::atomic<float> HeartTarget{ 0.f };
	std::atomic<float> WhisperTarget{ 0.f };
	std::atomic<float> StingStrength{ 1.f };
	std::atomic<int32> StingRequests{ 0 };
	std::atomic<float> BeepRate{ 0.f };
	std::atomic<float> BeepTarget{ 0.f };
	std::atomic<int32> ZapRequests{ 0 };
	std::atomic<float> MusicTarget{ 0.35f };

	TQueue<FSfxRequest, EQueueMode::Mpsc> SfxQueue;
	std::atomic<int32> SfxPending{ 0 }; // caps the queue if the audio device never pulls (e.g. -nosound)

	// Audio-thread state.
	uint32 Rng = 0x9E3779B9u;
	double Time = 0.0;
	float Ambience = 0.f, Heart = 0.f, Whisper = 0.f;
	float Brown = 0.f, WindLp = 0.f, WindGust = 0.f, WindGustTarget = 0.3f;
	int32 GustCountdown = 0;
	double BeatClock = 0.0;
	float Thump1 = 10.f, Thump2 = 10.f;      // seconds since each thump started
	float WhisperLp = 0.f, WhisperHp = 0.f, WhisperEnv = 0.f, WhisperEnvTarget = 0.f;
	int32 SyllableCountdown = 0;
	int32 StingsSeen = 0;
	float StingT = 10.f, StingAmp = 0.f, StingNoiseLp = 0.f;
	double BeepClock = 0.0;
	float BeepT = 10.f, Beep = 0.f;
	int32 ZapsSeen = 0;
	float ZapT = 10.f, ZapLp = 0.f;
	float LimEnv = 0.f;

	FVoice Voices[MaxVoices];

	// Music.
	float Music = 0.f;
	TArray<float> KsBuf;               // KsStrings * KsMax
	int32 KsLength[KsStrings] = {};
	int32 KsPos[KsStrings] = {};
	float KsLoss[KsStrings] = {};
	float KsPrev[KsStrings] = {};
	float TanNext = 0.3f, TanDc = 0.f; // TanNext: seconds until the next pluck
	int32 TanStep = 0;

	float FlPhase = 0.f, FlFreq = 277.f, FlTarget = 277.f, FlEnv = 0.f, FlEnvTarget = 0.f;
	float FlNoteT = 0.f, FlNoteLen = 0.f, FlWait = 2.5f, FlBreathLp = 0.f, FlAndolan = 0.f;
	int32 FlIndex = 1, FlNotesLeft = 0;
	bool bFlPlaying = false, bFlLast = false;

	TArray<float> EchoBuf;
	int32 EchoPos = 0;
	float EchoLp = 0.f;
};
