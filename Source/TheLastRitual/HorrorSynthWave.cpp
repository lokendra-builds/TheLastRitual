#include "HorrorSynthWave.h"

#include "GhostHunterCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

namespace HorrorSynth
{
	constexpr float Fs = 48000.f;
	constexpr float TwoPi = 2.f * PI;

	// Length (s) of each one-shot, in EHorrorSfx order.
	constexpr float SfxLen[] = {
		0.30f, // Footstep
		1.90f, // DoorOpen
		1.00f, // DoorClose
		0.60f, // DoorLocked
		2.20f, // DoorSlam
		1.20f, // Pickup
		4.00f, // ClueFound
		0.85f, // MirrorCover
		1.20f, // AlmirahIn
		1.20f, // AlmirahOut
		0.15f, // TorchClick
		0.60f, // BatteryPickup
		6.00f, // RitualCorrect
		4.50f, // RitualWrong
		0.07f, // UIClick
		1.60f, // Purchase
	};
	static_assert(UE_ARRAY_COUNT(SfxLen) == static_cast<int32>(EHorrorSfx::Count), "SfxLen must match EHorrorSfx");

	// Per-effect output gain (balances the raw generators; footsteps/SFX sit above the music).
	constexpr float SfxGain[] = {
		0.55f, // Footstep
		0.45f, // DoorOpen
		0.55f, // DoorClose
		0.50f, // DoorLocked
		0.55f, // DoorSlam
		0.45f, // Pickup
		0.30f, // ClueFound
		0.45f, // MirrorCover
		0.50f, // AlmirahIn
		0.50f, // AlmirahOut
		0.35f, // TorchClick
		0.40f, // BatteryPickup
		0.28f, // RitualCorrect
		0.35f, // RitualWrong
		0.20f, // UIClick
		0.35f, // Purchase
	};
	static_assert(UE_ARRAY_COUNT(SfxGain) == static_cast<int32>(EHorrorSfx::Count), "SfxGain must match EHorrorSfx");

	struct FPartial { float Ratio, Amp, Decay; };

	template <int32 N>
	float Bell(float t, float F0, const FPartial (&Partials)[N])
	{
		float S = 0.f;
		for (const FPartial& P : Partials)
		{
			S += P.Amp * FMath::Exp(-t / P.Decay) * FMath::Sin(TwoPi * F0 * P.Ratio * t);
		}
		return S * FMath::Min(1.f, t / 0.003f);
	}

	/** Chamberlin state-variable band-pass. Damp ~ 1/Q. */
	float BandPass(float In, float& Lp, float& Bp, float Fc, float Damp)
	{
		const float F = FMath::Min(2.f * FMath::Sin(PI * Fc / Fs), 0.95f);
		Lp += F * Bp;
		const float Hp = In - Lp - Damp * Bp;
		Bp += F * Hp;
		return Bp;
	}

	/** Small click / tick: noise spike + short ring. tl < 0 -> silent. */
	float Click(float tl, float Freq, float N)
	{
		if (tl < 0.f || tl > 0.05f)
		{
			return 0.f;
		}
		return N * FMath::Exp(-tl / 0.0012f)
			+ 0.6f * FMath::Sin(TwoPi * Freq * tl) * FMath::Exp(-tl / 0.004f)
			+ 0.3f * FMath::Sin(TwoPi * Freq * 0.31f * tl) * FMath::Exp(-tl / 0.008f);
	}

	/** Metallic coin clink (inharmonic partials). */
	float Clink(float tl, float C, float N)
	{
		if (tl < 0.f || tl > 0.6f)
		{
			return 0.f;
		}
		return 0.5f * FMath::Sin(TwoPi * 2350.f * C * tl) * FMath::Exp(-tl / 0.18f)
			+ 0.3f * FMath::Sin(TwoPi * 4890.f * C * tl) * FMath::Exp(-tl / 0.10f)
			+ 0.2f * FMath::Sin(TwoPi * 7320.f * C * tl) * FMath::Exp(-tl / 0.05f)
			+ 0.3f * N * FMath::Exp(-tl / 0.002f);
	}

	/** A soft sine chime. */
	float Chime(float tl, float Freq, float Decay)
	{
		if (tl < 0.f)
		{
			return 0.f;
		}
		return (FMath::Sin(TwoPi * Freq * tl) + 0.15f * FMath::Sin(TwoPi * Freq * 2.76f * tl) * FMath::Exp(-tl / (Decay * 0.3f)))
			* FMath::Exp(-tl / Decay) * FMath::Min(1.f, tl / 0.004f);
	}

	// Raga Bhairav around Sa: Ni(low) Sa re Ga ma Pa dha Ni Sa'
	constexpr float BhairavSemis[] = { -1.f, 0.f, 1.f, 4.f, 5.f, 7.f, 8.f, 11.f, 12.f };
	constexpr int32 BhairavNum = UE_ARRAY_COUNT(BhairavSemis);
	constexpr float SaHz = 138.59f; // C#3 tanpura Sa; flute plays an octave up
}

using namespace HorrorSynth;

UHorrorSynthWave::UHorrorSynthWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetSampleRate(Rate);
	NumChannels = 1;
	Duration = INDEFINITELY_LOOPING_DURATION;
	bLooping = false;
	SoundGroup = SOUNDGROUP_Default;

	// Tanpura strings: Pa (below Sa), Sa, Sa (a hair sharp), low Sa.
	const float StringHz[KsStrings] = { SaHz * 0.7492f, SaHz, SaHz * 1.0012f, SaHz * 0.5f };
	const float StringT60[KsStrings] = { 5.f, 5.f, 5.f, 6.5f };
	for (int32 s = 0; s < KsStrings; ++s)
	{
		KsLength[s] = FMath::Clamp(FMath::RoundToInt(Fs / StringHz[s] - 0.45f), 16, KsMax);
		KsLoss[s] = FMath::Exp(-6.9f / (StringHz[s] * StringT60[s]));
	}
	KsBuf.SetNumZeroed(KsStrings * KsMax);
	EchoBuf.SetNumZeroed(EchoLen);
}

float UHorrorSynthWave::Noise()
{
	// xorshift32 -> [-1, 1)
	Rng ^= Rng << 13;
	Rng ^= Rng >> 17;
	Rng ^= Rng << 5;
	return static_cast<float>(Rng) * (2.f / 4294967296.f) - 1.f;
}

// ---------------------------------------------------------------- Game-thread API

void UHorrorSynthWave::PlaySfx(EHorrorSfx Sfx, float Volume)
{
	if (Sfx >= EHorrorSfx::Count || Volume <= 0.f || SfxPending.load() > 64)
	{
		return;
	}
	SfxPending.fetch_add(1);
	SfxQueue.Enqueue(FSfxRequest{ Sfx, FMath::Clamp(Volume, 0.f, 2.f) });
	if (Sfx == EHorrorSfx::RitualWrong)
	{
		TriggerSting(0.55f * FMath::Min(Volume, 1.f)); // layer the existing dissonant sting
	}
}

UHorrorSynthWave* UHorrorSynthWave::Get(const UObject* WorldContext)
{
	if (!WorldContext || !WorldContext->GetWorld())
	{
		return nullptr;
	}
	const AGhostHunterCharacter* Hunter = Cast<AGhostHunterCharacter>(UGameplayStatics::GetPlayerCharacter(WorldContext, 0));
	return Hunter ? Hunter->GetHorrorAudio() : nullptr;
}

void UHorrorSynthWave::Play(const UObject* WorldContext, EHorrorSfx Sfx, float Volume)
{
	if (!WorldContext || !WorldContext->GetWorld())
	{
		return;
	}
	const AGhostHunterCharacter* Hunter = Cast<AGhostHunterCharacter>(UGameplayStatics::GetPlayerCharacter(WorldContext, 0));
	if (!Hunter)
	{
		return;
	}
	const bool bUi = Sfx == EHorrorSfx::UIClick || Sfx == EHorrorSfx::Purchase;
	UHorrorSynthWave* Synth = bUi && Hunter->GetUiAudio() ? Hunter->GetUiAudio() : Hunter->GetHorrorAudio();
	if (Synth)
	{
		Synth->PlaySfx(Sfx, Volume);
	}
}

void UHorrorSynthWave::PlayAt(const UObject* WorldContext, EHorrorSfx Sfx, const FVector& Where, float Volume, float MinVolume)
{
	if (!WorldContext || !WorldContext->GetWorld())
	{
		return;
	}
	const AGhostHunterCharacter* Hunter = Cast<AGhostHunterCharacter>(UGameplayStatics::GetPlayerCharacter(WorldContext, 0));
	UHorrorSynthWave* Synth = Hunter ? Hunter->GetHorrorAudio() : nullptr;
	if (!Synth)
	{
		return;
	}
	const float Dist = FVector::Dist(Hunter->GetActorLocation(), Where);
	const float Atten = FMath::GetMappedRangeValueClamped(FVector2D(400.f, 2500.f), FVector2D(1.f, 0.1f), Dist);
	Synth->PlaySfx(Sfx, Volume * FMath::Max(Atten, MinVolume));
}

// ---------------------------------------------------------------- Voices

void UHorrorSynthWave::StartVoice(EHorrorSfx Sfx, float Volume)
{
	FVoice* Slot = nullptr;
	float Oldest = -1.f;
	for (FVoice& V : Voices)
	{
		if (!V.bActive)
		{
			Slot = &V;
			Oldest = 2.f; // beats any active voice
		}
		else if (Oldest < 2.f && V.T / FMath::Max(V.Len, 0.01f) > Oldest)
		{
			Oldest = V.T / FMath::Max(V.Len, 0.01f);
			Slot = &V; // steal the voice closest to finishing
		}
	}
	if (!Slot)
	{
		return;
	}

	FVoice& V = *Slot;
	V = FVoice();
	V.Type = Sfx;
	V.bActive = true;
	V.Len = SfxLen[static_cast<int32>(Sfx)];
	V.Vol = Volume * SfxGain[static_cast<int32>(Sfx)];
	V.Pitch = 1.f + (Sfx == EHorrorSfx::Footstep ? 0.12f : 0.05f) * Noise();
	V.Bright = Rand01();
	for (float& R : V.R)
	{
		R = Rand01();
	}
}

float UHorrorSynthWave::Creak(FVoice& V, float t, float Dur, float BaseRate, float Res1, float Res2, float Dt)
{
	// Stick-slip friction: an irregular pulse train whose rate wobbles slowly,
	// rung through two wooden body resonances.
	float Impulse = 0.f;
	float Friction = 0.f;
	if (t >= 0.f && t < Dur)
	{
		const float U = t / Dur;
		const float Hump = FMath::Pow(FMath::Sin(PI * U), 0.7f) * (0.75f + 0.25f * FMath::Sin(TwoPi * (2.2f + V.R[2]) * t));
		const float PulseRate = BaseRate * V.Pitch
			* (1.f + 0.45f * FMath::Sin(TwoPi * (0.7f + 0.5f * V.R[0]) * t + 6.f * V.R[1]) + 0.2f * FMath::Sin(TwoPi * 3.1f * t))
			* (1.f + 0.2f * Noise());
		V.Ph[0] += FMath::Max(PulseRate, 8.f) * Dt;
		if (V.Ph[0] >= 1.f)
		{
			V.Ph[0] -= 1.f;
			Impulse = (0.6f + 0.4f * Noise()) * Hump;
		}
		Friction = 0.04f * Noise() * Hump;
	}
	const float In = Impulse * 10.f + Friction;
	return BandPass(In, V.F[4], V.F[5], Res1 * V.Pitch, 0.06f) + 0.6f * BandPass(In, V.F[6], V.F[7], Res2 * V.Pitch, 0.1f);
}

float UHorrorSynthWave::RenderVoice(FVoice& V, float Dt)
{
	const float t = V.T;
	const float N = Noise();
	const float P = V.Pitch;

	// Low wooden / stone thud at local time tl (uses Ph[PhI] and F[FI]).
	auto Thud = [&V, N, Dt](float tl, float F0, float Decay, float NoiseAmt, int32 PhI, int32 FI)
	{
		if (tl < 0.f || tl > Decay * 7.f)
		{
			return 0.f;
		}
		const float Freq = F0 * (1.f + 1.5f * FMath::Exp(-tl / 0.015f));
		V.Ph[PhI] += Freq * Dt;
		V.F[FI] += (N - V.F[FI]) * 0.15f;
		return FMath::Min(1.f, tl / 0.0015f) * (FMath::Exp(-tl / Decay) * FMath::Sin(TwoPi * V.Ph[PhI])
			+ NoiseAmt * V.F[FI] * FMath::Exp(-tl / 0.03f));
	};

	switch (V.Type)
	{
	case EHorrorSfx::Footstep:
	{
		// Heel on stone: pitch-dropping low body + gritty filtered noise + a soft scuff.
		const float Freq = 70.f * P * (1.f + 1.2f * FMath::Exp(-t / 0.01f));
		V.Ph[0] += Freq * Dt;
		const float Body = FMath::Min(1.f, t / 0.0015f) * FMath::Exp(-t / 0.055f) * FMath::Sin(TwoPi * V.Ph[0]);
		V.F[0] += (N - V.F[0]) * (0.05f + 0.25f * V.Bright);
		const float Grit = V.F[0] * FMath::Exp(-t / 0.018f) * FMath::Min(1.f, t / 0.001f);
		V.F[1] += (N - V.F[1]) * 0.02f;
		const float Rumble = 3.f * V.F[1] * FMath::Exp(-t / 0.05f);
		const float Ts = t - (0.04f + 0.04f * V.R[0]);
		const float Scuff = Ts > 0.f ? (N - V.F[0]) * FMath::Exp(-Ts / 0.025f) * FMath::Min(1.f, Ts / 0.005f) * 0.12f * (0.5f + V.Bright) : 0.f;
		return 0.9f * Body + 0.8f * Grit + 0.5f * Rumble + Scuff;
	}

	case EHorrorSfx::DoorOpen:
	{
		const float Dur = 1.2f + 0.4f * V.R[3];
		return Creak(V, t - 0.05f, Dur, 55.f, 650.f, 1500.f, Dt) + 0.4f * Click(t, 2500.f * P, N);
	}

	case EHorrorSfx::DoorClose:
		return Creak(V, t, 0.35f, 80.f, 700.f, 1600.f, Dt)
			+ 0.9f * Thud(t - 0.38f, 55.f * P, 0.12f, 0.6f, 1, 0)
			+ 0.35f * Click(t - 0.40f, 2900.f * P, N);

	case EHorrorSfx::DoorLocked:
	{
		// Handle rattling against the latch.
		constexpr float Times[] = { 0.f, 0.075f, 0.16f, 0.27f, 0.36f };
		constexpr float Amps[] = { 1.f, 0.6f, 0.9f, 0.5f, 0.7f };
		float S = 0.f;
		for (int32 k = 0; k < UE_ARRAY_COUNT(Times); ++k)
		{
			const float Tl = t - Times[k] - 0.015f * V.R[k % 4];
			if (Tl < 0.f || Tl > 0.12f)
			{
				continue;
			}
			S += Amps[k] * (0.25f * FMath::Sin(TwoPi * 3100.f * P * Tl) * FMath::Exp(-Tl / 0.025f)
				+ 0.15f * FMath::Sin(TwoPi * 4700.f * P * Tl) * FMath::Exp(-Tl / 0.015f)
				+ 0.6f * FMath::Sin(TwoPi * 190.f * P * Tl) * FMath::Exp(-Tl / 0.02f)
				+ 0.5f * N * FMath::Exp(-Tl / 0.004f));
		}
		return S;
	}

	case EHorrorSfx::DoorSlam:
	{
		const float Freq = 38.f * P * (1.f + 2.f * FMath::Exp(-t / 0.03f));
		V.Ph[0] += Freq * Dt;
		const float Boom = FMath::Min(1.f, t / 0.002f) * FMath::Exp(-t / 0.4f) * FMath::Sin(TwoPi * V.Ph[0]);
		V.F[0] += (N - V.F[0]) * 0.4f;
		const float Crack = 1.2f * V.F[0] * FMath::Exp(-t / 0.035f);
		V.F[1] += (N - V.F[1]) * 0.015f;
		const float Tail = 4.f * V.F[1] * FMath::Exp(-t / 0.7f);
		const float Rattle = 0.4f * (Click(t - 0.06f, 3100.f, N) + 0.7f * Click(t - 0.11f, 2700.f, N) + 0.5f * Click(t - 0.19f, 3300.f, N));
		return Boom + Crack + Tail + Rattle;
	}

	case EHorrorSfx::Pickup:
	{
		// Paper / cloth rustle in three little gestures, then a small chime.
		constexpr float Starts[] = { 0.f, 0.09f, 0.2f };
		float Env = 0.f;
		for (float S0 : Starts)
		{
			const float U = (t - S0) / 0.07f;
			if (U > 0.f && U < 1.f)
			{
				Env += FMath::Sin(PI * U);
			}
		}
		V.F[0] += (N - V.F[0]) * 0.25f;
		const float Hp = N - V.F[0];
		const float Spark = Rand01() > 0.85f ? 1.f : 0.25f;
		return 0.9f * Hp * Spark * Env
			+ 0.25f * Chime(t - 0.16f, 1568.f * P, 0.45f) + 0.12f * Chime(t - 0.16f, 2349.f * P, 0.3f);
	}

	case EHorrorSfx::ClueFound:
	{
		// Soft low ghanta: hum, beating fundamental, minor-third partial and a few uppers.
		static const FPartial Ghanta[] = {
			{ 0.5f, 0.4f, 3.0f }, { 1.f, 1.f, 2.2f }, { 1.004f, 0.6f, 2.2f }, { 1.19f, 0.45f, 1.4f },
			{ 1.5f, 0.3f, 1.1f }, { 2.f, 0.25f, 0.9f }, { 2.62f, 0.15f, 0.6f }, { 3.3f, 0.08f, 0.4f } };
		V.F[0] += (N - V.F[0]) * 0.1f;
		return 0.5f * Bell(t, 180.f * P, Ghanta) + 0.3f * V.F[0] * FMath::Exp(-t / 0.01f);
	}

	case EHorrorSfx::MirrorCover:
	{
		const float U = t / 0.55f;
		float S = 0.f;
		if (U < 1.f)
		{
			const float H = FMath::Sin(PI * U);
			const float Fc = 400.f + 2600.f * H * H * (0.7f + 0.3f * V.Bright);
			S = 1.6f * BandPass(N, V.F[0], V.F[1], Fc, 1.2f) * FMath::Pow(H, 1.5f);
		}
		return S + 0.35f * Thud(t - 0.45f, 90.f * P, 0.05f, 0.3f, 0, 2);
	}

	case EHorrorSfx::AlmirahIn:
		return Creak(V, t, 0.55f, 45.f, 420.f, 1100.f, Dt)
			+ Thud(t - 0.6f, 75.f * P, 0.1f, 0.6f, 1, 0)
			+ 0.3f * Click(t - 0.63f, 2600.f * P, N);

	case EHorrorSfx::AlmirahOut:
		return 0.35f * Click(t, 2600.f * P, N)
			+ Creak(V, t - 0.05f, 0.7f, 40.f, 420.f, 1100.f, Dt)
			+ 0.6f * Thud(t - 0.82f, 65.f * P, 0.05f, 0.8f, 1, 0);

	case EHorrorSfx::TorchClick:
		return Click(t, 2600.f * P, N) + 0.6f * Click(t - 0.06f - 0.015f * V.R[0], 3300.f * P, N);

	case EHorrorSfx::BatteryPickup:
	{
		float S = Click(t, 2000.f * P, N) + 0.7f * Click(t - 0.045f, 1500.f * P, N) + Click(t - 0.22f, 3500.f * P, N);
		const float Tb = t - 0.28f;
		if (Tb > 0.f && Tb < 0.15f)
		{
			V.Ph[0] += (900.f + 700.f * Tb / 0.15f) * Dt;
			S += 0.25f * FMath::Sin(PI * Tb / 0.15f) * FMath::Sin(TwoPi * V.Ph[0]);
		}
		return S;
	}

	case EHorrorSfx::RitualCorrect:
	{
		// Bright temple bell + a harmonium-like Sa Ga Pa Sa' chord rising in.
		static const FPartial Temple[] = {
			{ 0.5f, 0.25f, 3.5f }, { 1.f, 1.f, 3.0f }, { 2.f, 0.45f, 2.2f }, { 2.76f, 0.35f, 1.4f },
			{ 3.01f, 0.25f, 1.4f }, { 4.17f, 0.18f, 0.9f }, { 5.43f, 0.12f, 0.6f } };
		float S = 0.6f * Bell(t, 523.f * P, Temple);
		constexpr float ChordSemis[] = { 0.f, 4.f, 7.f, 12.f };
		for (int32 k = 0; k < 4; ++k)
		{
			const float Tl = t - (0.35f + 0.5f * k);
			if (Tl < 0.f)
			{
				continue;
			}
			const float Freq = 2.f * SaHz * FMath::Pow(2.f, ChordSemis[k] / 12.f);
			V.Ph[k] += Freq * Dt;
			V.Ph[k] -= FMath::FloorToFloat(V.Ph[k]);
			const float X = TwoPi * V.Ph[k];
			const float Reed = FMath::Sin(X) + 0.5f * FMath::Sin(2.f * X) + 0.33f * FMath::Sin(3.f * X) + 0.2f * FMath::Sin(4.f * X);
			const float Env = FMath::Min(1.f, Tl / 0.25f) * FMath::Clamp((V.Len - 0.2f - t) / 1.5f, 0.f, 1.f);
			S += 0.22f * Reed * Env;
		}
		return S;
	}

	case EHorrorSfx::RitualWrong:
	{
		// Low cluster of detuned saws (semitone + tritone), low-passed, with a boom.
		constexpr float Cluster[] = { 65.4f, 69.3f, 92.5f, 98.f };
		float Saw = 0.f;
		for (int32 k = 0; k < 4; ++k)
		{
			V.Ph[k] += Cluster[k] * P * (1.f - 0.01f * t) * Dt;
			V.Ph[k] -= FMath::FloorToFloat(V.Ph[k]);
			Saw += 2.f * V.Ph[k] - 1.f;
		}
		V.F[0] += (Saw - V.F[0]) * 0.06f;
		V.F[1] += (V.F[0] - V.F[1]) * 0.06f;
		const float Env = FMath::Min(1.f, t / 0.005f) * FMath::Exp(-t / 1.4f);
		const float BoomF = 30.f * (1.f + 2.f * FMath::Exp(-t / 0.05f));
		V.Ph[4] += BoomF * Dt;
		V.F[2] += (N - V.F[2]) * 0.2f;
		return 0.5f * V.F[1] * Env + FMath::Exp(-t / 0.6f) * FMath::Sin(TwoPi * V.Ph[4]) * FMath::Min(1.f, t / 0.003f)
			+ 0.6f * V.F[2] * FMath::Exp(-t / 0.05f);
	}

	case EHorrorSfx::UIClick:
		return FMath::Sin(TwoPi * 1350.f * P * t) * FMath::Exp(-t / 0.012f) + 0.4f * N * FMath::Exp(-t / 0.0015f);

	case EHorrorSfx::Purchase:
		return Clink(t, 1.f, N) + 0.8f * Clink(t - 0.1f - 0.03f * V.R[0], 1.07f, N) + 0.7f * Clink(t - 0.2f - 0.04f * V.R[1], 0.94f, N)
			+ 0.3f * Chime(t - 0.3f, 1047.f, 0.7f) + 0.3f * Chime(t - 0.42f, 1568.f, 0.7f);

	default:
		return 0.f;
	}
}

// ---------------------------------------------------------------- Music

void UHorrorSynthWave::PluckString(int32 Index, float Strength)
{
	// Finger pluck: a triangle (string pulled at 1/5 of its length; harmonics fall as 1/n^2,
	// so the fundamental is strong) plus a little noise for the attack.
	float* Buf = &KsBuf[Index * KsMax];
	const int32 L = KsLength[Index];
	const float Apex = 0.2f * L;
	float Lp = 0.f;
	for (int32 i = 0; i < L; ++i)
	{
		const float Tri = i < Apex ? i / Apex : (L - i) / (L - Apex);
		Lp += (Noise() - Lp) * 0.5f;
		Buf[i] = 0.3f * Buf[i] + Strength * (Tri - 0.5f + 0.25f * Lp);
	}
	KsPrev[Index] = 0.f;
}

void UHorrorSynthWave::StartFluteNote(bool bFirst, bool bLast)
{
	if (!bFirst)
	{
		const float R = Rand01();
		const int32 Step = R < 0.35f ? -1 : R < 0.7f ? 1 : R < 0.82f ? -2 : R < 0.94f ? 2 : 0;
		FlIndex = FMath::Clamp(FlIndex + Step, 0, BhairavNum - 1);
	}
	if (bLast)
	{
		// Phrases come to rest on Sa, Ga, Pa or Sa'.
		constexpr int32 Rest[] = { 1, 3, 5, 8 };
		int32 Best = Rest[0];
		for (int32 Ri : Rest)
		{
			if (FMath::Abs(Ri - FlIndex) < FMath::Abs(Best - FlIndex))
			{
				Best = Ri;
			}
		}
		FlIndex = Best;
	}

	const float Semi = BhairavSemis[FlIndex];
	FlTarget = 2.f * SaHz * FMath::Pow(2.f, Semi / 12.f);
	if (bFirst)
	{
		FlFreq = FlTarget * 0.96f; // scoop up into the first note
	}
	// Komal re and dha are sung with a slow oscillation (andolan) in Bhairav.
	FlAndolan = (Semi == 1.f || Semi == 8.f) ? 1.f : 0.f;
	FlNoteT = 0.f;
	FlNoteLen = bLast ? 2.6f + 1.6f * Rand01() : 1.0f + 1.3f * Rand01();
	bFlLast = bLast;
	--FlNotesLeft;
}

float UHorrorSynthWave::RenderMusic(float Dt)
{
	// ---- Tanpura: Pa, Sa, Sa, low Sa, then a breath; repeat.
	TanNext -= Dt;
	if (TanNext <= 0.f)
	{
		constexpr float Gaps[KsStrings] = { 1.25f, 1.15f, 1.1f, 1.7f };
		constexpr float Strength[KsStrings] = { 0.85f, 1.f, 0.9f, 1.f };
		PluckString(TanStep, Strength[TanStep]);
		TanNext += Gaps[TanStep] + 0.06f * Noise();
		TanStep = (TanStep + 1) % KsStrings;
	}

	float Tan = 0.f;
	for (int32 s = 0; s < KsStrings; ++s)
	{
		float* Buf = &KsBuf[s * KsMax];
		const int32 Pos = KsPos[s];
		const float X = Buf[Pos];
		Buf[Pos] = KsLoss[s] * (0.7f * X + 0.3f * KsPrev[s]); // brighter loop filter keeps the overtones ringing
		KsPrev[s] = X;
		KsPos[s] = Pos + 1 >= KsLength[s] ? 0 : Pos + 1;
		Tan += X + 0.2f * FMath::Abs(X); // rectified part = jawari-like buzz (rich even overtones)
	}
	TanDc += (Tan - TanDc) * 0.0005f;
	Tan -= TanDc;

	// ---- Bansuri-like melody: a short phrase, then a long silence.
	if (!bFlPlaying)
	{
		FlWait -= Dt;
		if (FlWait <= 0.f)
		{
			bFlPlaying = true;
			FlNotesLeft = 2 + static_cast<int32>(Rand01() * 3.99f);
			const float R = Rand01();
			FlIndex = R < 0.45f ? 1 : (R < 0.7f ? 3 : 5);
			StartFluteNote(true, FlNotesLeft == 1);
		}
	}
	else
	{
		FlNoteT += Dt;
		if (FlNoteT >= FlNoteLen)
		{
			if (FlNotesLeft > 0)
			{
				StartFluteNote(false, FlNotesLeft == 1);
			}
			else
			{
				bFlPlaying = false;
				FlWait = 6.f + 8.f * Rand01();
			}
		}
	}

	float EnvTarget = 0.f;
	if (bFlPlaying)
	{
		EnvTarget = FlNoteT < 0.06f ? 0.65f : 1.f; // tiny articulation dip between notes
		if (bFlLast && FlNoteT > FlNoteLen - 1.2f)
		{
			EnvTarget = 0.f; // let the last note die away
		}
	}
	FlEnv += (EnvTarget - FlEnv) * (Dt / (EnvTarget > FlEnv ? 0.15f : 0.45f));
	FlFreq += (FlTarget - FlFreq) * (Dt / 0.09f); // meend: glide between notes

	float Flute = 0.f;
	if (FlEnv > 1e-4f)
	{
		const float Vib = FlAndolan > 0.f
			? 0.012f * FMath::Sin(TwoPi * 1.3f * FlNoteT)
			: 0.004f * FMath::Min(1.f, FlNoteT / 0.7f) * FMath::Sin(TwoPi * 5.2f * FlNoteT);
		FlPhase += FlFreq * (1.f + Vib) * Dt;
		FlPhase -= FMath::FloorToFloat(FlPhase);
		const float X = TwoPi * FlPhase;
		const float Tone = FMath::Sin(X) + 0.2f * FMath::Sin(2.f * X) + 0.06f * FMath::Sin(3.f * X);
		const float N = Noise();
		FlBreathLp += (N - FlBreathLp) * 0.3f;
		const float Hiss = N - FlBreathLp;
		const float Chiff = FlNoteT < 0.08f ? 0.25f * (1.f - FlNoteT / 0.08f) : 0.f;
		Flute = FlEnv * (Tone + Hiss * (0.08f + Chiff));
	}

	// ---- Soft echo (one feedback delay, darkened each pass).
	const float Echo = EchoBuf[EchoPos];
	EchoLp += (Echo - EchoLp) * 0.3f;
	EchoBuf[EchoPos] = 0.5f * Flute + 0.15f * Tan + 0.42f * EchoLp;
	EchoPos = EchoPos + 1 >= EchoLen ? 0 : EchoPos + 1;

	return 0.12f * Tan + 0.13f * Flute + 0.12f * EchoLp;
}

// ---------------------------------------------------------------- Horror bed

float UHorrorSynthWave::RenderBed(float N, float Dt, float BeatPeriod, float Smooth, float BeepR, float BeepLevelT)
{
	const float T = static_cast<float>(Time);

	// ---- Ambience: brown-noise rumble, two detuned low sines, and gusting wind.
	Brown = FMath::Clamp(Brown + N * 0.02f, -1.f, 1.f) * 0.998f;
	const float Drone = 0.5f * FMath::Sin(2.f * PI * 55.f * T) + 0.4f * FMath::Sin(2.f * PI * 55.7f * T)
		+ 0.25f * FMath::Sin(2.f * PI * 82.4f * T + 0.7f * FMath::Sin(2.f * PI * 0.07f * T));
	const float DroneLfo = 0.6f + 0.4f * FMath::Sin(2.f * PI * 0.05f * T);

	if (--GustCountdown <= 0)
	{
		WindGustTarget = 0.1f + 0.9f * FMath::Square(0.5f + 0.5f * Noise());
		GustCountdown = static_cast<int32>(Rate * (1.5f + 2.5f * (0.5f + 0.5f * Noise())));
	}
	WindGust += (WindGustTarget - WindGust) * (1.f / (Rate * 1.2f));
	const float WindCut = 0.004f + 0.03f * WindGust; // gust opens the filter: "whooo"
	WindLp += (N - WindLp) * WindCut;

	float S = Ambience * (0.10f * Brown + 0.035f * Drone * DroneLfo + 0.9f * WindLp * WindGust);

	// ---- Heartbeat: lub (t=0) and dub (t=0.22 s), pitch-dropping low thumps.
	BeatClock += Dt;
	if (BeatClock >= BeatPeriod)
	{
		BeatClock -= BeatPeriod;
		Thump1 = 0.f;
	}
	if (Thump1 < 10.f) Thump1 += Dt;
	if (FMath::IsNearlyEqual(Thump1, 0.22f, Dt * 0.6f)) Thump2 = 0.f;
	if (Thump2 < 10.f) Thump2 += Dt;

	auto ThumpAt = [](float t, float Gain)
	{
		if (t > 0.35f) return 0.f;
		const float F = 40.f + 35.f * FMath::Exp(-t / 0.03f);
		return Gain * FMath::Exp(-t / 0.07f) * FMath::Sin(2.f * PI * F * t);
	};
	S += Heart * 0.9f * (ThumpAt(Thump1, 1.f) + ThumpAt(Thump2, 0.7f));

	// ---- Whisper: band-passed noise in random syllable bursts.
	if (--SyllableCountdown <= 0)
	{
		WhisperEnvTarget = (Noise() > -0.2f) ? 0.4f + 0.6f * (0.5f + 0.5f * Noise()) : 0.f;
		SyllableCountdown = static_cast<int32>(Rate * (0.06f + 0.16f * (0.5f + 0.5f * Noise())));
	}
	WhisperEnv += (WhisperEnvTarget - WhisperEnv) * 0.003f;
	WhisperLp += (N - WhisperLp) * 0.35f;          // ~3 kHz low-pass
	WhisperHp += (WhisperLp - WhisperHp) * 0.06f;  // remove lows -> band-pass
	S += Whisper * 0.5f * WhisperEnv * (WhisperLp - WhisperHp);

	// ---- Sting: dissonant cluster + noise burst + low boom.
	if (StingT < 4.f)
	{
		const float Env = StingAmp * FMath::Exp(-StingT / 1.1f) * FMath::Min(1.f, StingT / 0.01f);
		const float Cluster = FMath::Sin(2.f * PI * 233.f * StingT) + FMath::Sin(2.f * PI * 247.f * StingT)
			+ 0.7f * FMath::Sin(2.f * PI * 349.f * StingT) + 0.5f * FMath::Sin(2.f * PI * 466.f * StingT * (1.f + 0.02f * StingT));
		StingNoiseLp += (N - StingNoiseLp) * 0.2f;
		const float Boom = FMath::Exp(-StingT / 0.5f) * FMath::Sin(2.f * PI * (35.f + 25.f * FMath::Exp(-StingT / 0.1f)) * StingT);
		S += Env * (0.12f * Cluster + 0.5f * StingNoiseLp) + StingAmp * 0.8f * Boom;
		StingT += Dt;
	}

	// ---- EMF pips: short 2.4 kHz beeps.
	Beep += (BeepLevelT - Beep) * Smooth;
	if (BeepR > 0.f)
	{
		BeepClock += Dt;
		if (BeepClock >= 1.0 / BeepR)
		{
			BeepClock = 0.0;
			BeepT = 0.f;
		}
	}
	if (BeepT < 0.05f)
	{
		S += Beep * 0.18f * FMath::Sin(2.f * PI * 2400.f * BeepT) * (1.f - BeepT / 0.05f);
		BeepT += Dt;
	}

	// ---- Gun Torch: crack + falling whine.
	if (ZapT < 0.35f)
	{
		ZapLp += (N - ZapLp) * 0.5f;
		const float F = 300.f + 2200.f * FMath::Exp(-ZapT / 0.05f);
		S += 0.6f * FMath::Exp(-ZapT / 0.06f) * ZapLp + 0.25f * FMath::Exp(-ZapT / 0.12f) * FMath::Sin(2.f * PI * F * ZapT);
		ZapT += Dt;
	}
	return S;
}

// ---------------------------------------------------------------- Mix

int32 UHorrorSynthWave::OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples)
{
	OutAudio.SetNumZeroed(NumSamples * sizeof(int16));
	int16* Out = reinterpret_cast<int16*>(OutAudio.GetData());

	const float Dt = 1.f / Rate;
	const float AmbT = AmbienceTarget.load();
	const float HeartT = HeartTarget.load();
	const float WhispT = WhisperTarget.load();
	const float Bpm = HeartBpm.load();
	const float MusicT = bSfxOnly ? 0.f : MusicTarget.load();
	const int32 Requests = StingRequests.load();
	if (Requests != StingsSeen)
	{
		StingsSeen = Requests;
		StingT = 0.f;
		StingAmp = StingStrength.load();
	}

	const int32 Zaps = ZapRequests.load();
	if (Zaps != ZapsSeen)
	{
		ZapsSeen = Zaps;
		ZapT = 0.f;
	}
	const float BeepR = BeepRate.load();
	const float BeepLevelT = BeepTarget.load();

	FSfxRequest Req;
	while (SfxQueue.Dequeue(Req))
	{
		SfxPending.fetch_sub(1);
		StartVoice(Req.Sfx, Req.Volume);
	}

	const float BeatPeriod = 60.f / Bpm;
	const float Smooth = 1.f - FMath::Exp(-Dt / 0.4f); // ~0.4 s fades between levels
	const float MusicSmooth = 1.f - FMath::Exp(-Dt / 2.f); // music fades over ~2 s
	const float LimRelease = FMath::Exp(-Dt / 0.3f);

	for (int32 i = 0; i < NumSamples; ++i)
	{
		float S = 0.f;
		if (!bSfxOnly)
		{
			Ambience += (AmbT - Ambience) * Smooth;
			Heart += (HeartT - Heart) * Smooth;
			Whisper += (WhispT - Whisper) * Smooth;
			S += RenderBed(Noise(), Dt, BeatPeriod, Smooth, BeepR, BeepLevelT);

			Music += (MusicT - Music) * MusicSmooth;
			if (Music > 1e-4f || MusicT > 0.f)
			{
				S += Music * RenderMusic(Dt);
			}
		}

		for (FVoice& V : Voices)
		{
			if (V.bActive)
			{
				S += V.Vol * RenderVoice(V, Dt);
				V.T += Dt;
				V.bActive = V.T < V.Len;
			}
		}

		// Peak limiter (instant attack, ~0.3 s release) so stacked hits duck instead of
		// hard-clipping, then the soft clip, then write.
		const float Peak = FMath::Abs(S);
		LimEnv = Peak > LimEnv ? Peak : LimEnv * LimRelease;
		if (LimEnv > 1.1f)
		{
			S *= 1.1f / LimEnv;
		}
		S = FMath::Clamp(S, -1.5f, 1.5f);
		S = S - (S * S * S) / 6.75f;
		Out[i] = static_cast<int16>(FMath::Clamp(S, -1.f, 1.f) * 30000.f);

		Time += Dt;
	}

	return NumSamples;
}

// ---------------------------------------------------------------- Offline test

void UHorrorSynthWave::RenderTestWav(const FString& WavPath, float MusicSeconds)
{
	UHorrorSynthWave* W = NewObject<UHorrorSynthWave>(GetTransientPackage());
	W->SetAmbienceLevel(0.f);
	W->SetMusicLevel(0.f);
	W->SetHeartbeat(70.f, 0.f);

	struct FEvent { double At; TFunction<void()> Fn; };
	TArray<FEvent> Events;
	FString Csv = TEXT("label,start,end\n");
	auto Segment = [&Csv](const FString& Label, double Start, double End)
	{
		Csv += FString::Printf(TEXT("%s,%.3f,%.3f\n"), *Label, Start, End);
	};
	auto Sfx = [&Events, W](double At, EHorrorSfx Fx, float Volume)
	{
		Events.Add({ At, [W, Fx, Volume]() { W->PlaySfx(Fx, Volume); } });
	};

	// 1) Footsteps alone: walk, run, crouch.
	double T = 0.5;
	for (int32 i = 0; i < 8; ++i) Sfx(T + 0.5 * i, EHorrorSfx::Footstep, 0.6f);
	Segment(TEXT("foot_walk"), T, T + 4.0);
	T += 4.5;
	for (int32 i = 0; i < 8; ++i) Sfx(T + 0.33 * i, EHorrorSfx::Footstep, 1.f);
	Segment(TEXT("foot_run"), T, T + 2.9);
	T += 3.5;
	for (int32 i = 0; i < 5; ++i) Sfx(T + 0.65 * i, EHorrorSfx::Footstep, 0.22f);
	Segment(TEXT("foot_crouch"), T, T + 3.5);
	T += 4.0;

	// 2) Every one-shot in sequence.
	static const TCHAR* Names[] = { TEXT("Footstep"), TEXT("DoorOpen"), TEXT("DoorClose"), TEXT("DoorLocked"), TEXT("DoorSlam"),
		TEXT("Pickup"), TEXT("ClueFound"), TEXT("MirrorCover"), TEXT("AlmirahIn"), TEXT("AlmirahOut"), TEXT("TorchClick"),
		TEXT("BatteryPickup"), TEXT("RitualCorrect"), TEXT("RitualWrong"), TEXT("UIClick"), TEXT("Purchase") };
	static_assert(UE_ARRAY_COUNT(Names) == static_cast<int32>(EHorrorSfx::Count), "Names must match EHorrorSfx");
	for (int32 k = 1; k < static_cast<int32>(EHorrorSfx::Count); ++k)
	{
		const float Len = SfxLen[k];
		Sfx(T, static_cast<EHorrorSfx>(k), 1.f);
		Segment(FString(TEXT("sfx_")) + Names[k], T, T + Len);
		T += FMath::Max(2.0, Len + 0.8);
	}

	// 3) Music alone at the normal level, then at the hunt level.
	Events.Add({ T, [W]() { W->SetMusicLevel(0.35f); } });
	Segment(TEXT("music"), T + 4.0, T + MusicSeconds);
	T += MusicSeconds;
	Events.Add({ T, [W]() { W->SetMusicLevel(0.05f); } });
	Segment(TEXT("music_hunt"), T + 8.0, T + 16.0);
	T += 16.0;

	// 4) Worst-case full mix: ambience, heartbeat, whispers, EMF, music, running, slam, wrong ritual, sting, zap.
	Events.Add({ T, [W]() { W->SetMusicLevel(0.35f); W->SetAmbienceLevel(0.75f); W->SetHeartbeat(150.f, 1.f); W->SetWhisperLevel(1.f); W->SetBeep(4.f, 0.5f); } });
	for (int32 i = 0; i < 36; ++i) Sfx(T + 0.33 * i, EHorrorSfx::Footstep, 1.f);
	Sfx(T + 2.0, EHorrorSfx::DoorSlam, 1.f);
	Sfx(T + 4.0, EHorrorSfx::RitualWrong, 1.f);
	Events.Add({ T + 6.0, [W]() { W->TriggerSting(1.2f); } });
	Events.Add({ T + 7.0, [W]() { W->TriggerZap(); } });
	Sfx(T + 8.0, EHorrorSfx::DoorOpen, 1.f);
	Sfx(T + 9.0, EHorrorSfx::RitualCorrect, 1.f);
	Segment(TEXT("full_mix"), T, T + 12.0);
	T += 13.0;

	Events.Sort([](const FEvent& A, const FEvent& B) { return A.At < B.At; });

	const int32 Total = static_cast<int32>(T * Rate);
	constexpr int32 Block = 256;
	TArray<int16> Pcm;
	Pcm.Reserve(Total);
	TArray<uint8> Chunk;
	int32 NextEvent = 0;
	for (int32 Done = 0; Done < Total; Done += Block)
	{
		const double Now = static_cast<double>(Done) / Rate;
		while (NextEvent < Events.Num() && Events[NextEvent].At <= Now)
		{
			Events[NextEvent++].Fn();
		}
		W->OnGeneratePCMAudio(Chunk, Block);
		Pcm.Append(reinterpret_cast<const int16*>(Chunk.GetData()), Block);
	}

	// Plain 16-bit mono WAV.
	TArray<uint8> Wav;
	auto U32 = [&Wav](uint32 V) { for (int32 b = 0; b < 4; ++b) Wav.Add(static_cast<uint8>(V >> (8 * b))); };
	auto U16 = [&Wav](uint16 V) { Wav.Add(static_cast<uint8>(V)); Wav.Add(static_cast<uint8>(V >> 8)); };
	auto Tag = [&Wav](const char* S4) { for (int32 b = 0; b < 4; ++b) Wav.Add(static_cast<uint8>(S4[b])); };
	const uint32 DataBytes = Pcm.Num() * sizeof(int16);
	Tag("RIFF"); U32(36 + DataBytes); Tag("WAVE");
	Tag("fmt "); U32(16); U16(1); U16(1); U32(Rate); U32(Rate * 2); U16(2); U16(16);
	Tag("data"); U32(DataBytes);
	Wav.Append(reinterpret_cast<const uint8*>(Pcm.GetData()), DataBytes);

	const bool bOk = FFileHelper::SaveArrayToFile(Wav, *WavPath)
		&& FFileHelper::SaveStringToFile(Csv, *FPaths::ChangeExtension(WavPath, TEXT("csv")));
	UE_LOG(LogTemp, Display, TEXT("HorrorSynth: wrote %.1f s test audio to %s (%s)"), T, *WavPath, bOk ? TEXT("ok") : TEXT("FAILED"));
}
