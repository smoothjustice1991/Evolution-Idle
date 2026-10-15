#include "EvolutionGameSubsystem.h"
#include "EvolutionIdle.h"
#include "EvolutionSaveGame.h"
#include "EvoSteam.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWaveProcedural.h"

const FString UEvolutionGameSubsystem::SaveSlotName = TEXT("EvolutionIdle_Save");

namespace
{
	constexpr int32 SaveUserIndex = 0;

	/** Longest single frame we will credit, so a debugger pause or hitch cannot produce absurd jumps. */
	constexpr double MaxTickSeconds = 60.0;

	/** How often click and production achievements are re-checked. */
	constexpr double AchievementCheckSeconds = 0.25;

	/** Music is queued this many seconds ahead of playback, in chunks of MusicChunkSeconds. */
	constexpr double MusicBufferSeconds = 1.5;
	constexpr double MusicChunkSeconds = 0.25;

	/**
	 * Minimal RIFF/WAVE reader: needs a 16-bit PCM "fmt " chunk and a "data" chunk.
	 * Appends the sample data to OutPCM and returns the format, or false if the file is not 16-bit PCM.
	 */
	bool ReadPcmWav(const FString& Path, TArray<uint8>& OutPCM, int32& OutSampleRate, int32& OutChannels)
	{
		TArray<uint8> File;
		if (!FFileHelper::LoadFileToArray(File, *Path))
		{
			return false;
		}
		auto ReadU16 = [&File](int32 Offset) { return static_cast<uint16>(File[Offset] | (File[Offset + 1] << 8)); };
		auto ReadU32 = [&File](int32 Offset)
		{
			return static_cast<uint32>(File[Offset]) | (static_cast<uint32>(File[Offset + 1]) << 8)
				| (static_cast<uint32>(File[Offset + 2]) << 16) | (static_cast<uint32>(File[Offset + 3]) << 24);
		};
		if (File.Num() < 12 || FMemory::Memcmp(File.GetData(), "RIFF", 4) != 0 || FMemory::Memcmp(File.GetData() + 8, "WAVE", 4) != 0)
		{
			return false;
		}
		int32 Offset = 12;
		uint16 Format = 0, Channels = 0, Bits = 0;
		uint32 Rate = 0;
		int32 DataStart = -1;
		int32 DataSize = 0;
		while (Offset + 8 <= File.Num())
		{
			const uint32 ChunkSize = ReadU32(Offset + 4);
			const int32 Body = Offset + 8;
			if (Body + static_cast<int64>(ChunkSize) > File.Num())
			{
				break;
			}
			if (FMemory::Memcmp(File.GetData() + Offset, "fmt ", 4) == 0 && ChunkSize >= 16)
			{
				Format = ReadU16(Body);
				Channels = ReadU16(Body + 2);
				Rate = ReadU32(Body + 4);
				Bits = ReadU16(Body + 14);
			}
			else if (FMemory::Memcmp(File.GetData() + Offset, "data", 4) == 0)
			{
				DataStart = Body;
				DataSize = static_cast<int32>(ChunkSize);
			}
			Offset = Body + static_cast<int32>(ChunkSize) + (ChunkSize & 1);
		}
		if (Format != 1 || Bits != 16 || Channels == 0 || Rate == 0 || DataStart < 0 || DataSize <= 0)
		{
			return false;
		}
		const int32 FrameBytes = Channels * 2;
		OutPCM.Append(File.GetData() + DataStart, DataSize - DataSize % FrameBytes);
		OutSampleRate = static_cast<int32>(Rate);
		OutChannels = Channels;
		return true;
	}
}

void UEvolutionGameSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LoadGameData();
	Economy.Init(&Data);
	LoadSounds();

	if (bDataValid)
	{
		LoadProgress();
		InitSteam();
	}

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UEvolutionGameSubsystem::TickEconomy), 0.0f);
}

void UEvolutionGameSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	TickHandle.Reset();

	if (bDataValid)
	{
		SaveNow();
	}

	if (Steam.IsValid())
	{
		Steam->Shutdown();
		Steam.Reset();
	}

	Super::Deinitialize();
}

void UEvolutionGameSubsystem::LoadGameData()
{
	const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data"), TEXT("GameData.json"));
	bDataValid = FEvoGameData::LoadFromFile(Path, Data, DataError);
	if (bDataValid)
	{
		UE_LOG(LogEvolution, Log, TEXT("Loaded %d stages and %d upgrades from %s"), Data.Stages.Num(), Data.Upgrades.Num(), *Path);
	}
	else
	{
		UE_LOG(LogEvolution, Error, TEXT("Game data failed to load: %s"), *DataError);
	}
}

void UEvolutionGameSubsystem::LoadProgress()
{
	if (!UGameplayStatics::DoesSaveGameExist(SaveSlotName, SaveUserIndex))
	{
		UE_LOG(LogEvolution, Log, TEXT("No save found; starting a new game."));
		SaveNow();
		return;
	}

	UEvolutionSaveGame* Save = Cast<UEvolutionSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, SaveUserIndex));
	if (!Save)
	{
		UE_LOG(LogEvolution, Warning, TEXT("Save slot exists but could not be read; starting a new game."));
		SaveNow();
		return;
	}

	FEvoState& State = Economy.GetState();
	State.CurrentEP = Save->CurrentEP;
	State.LifetimeEP = Save->LifetimeEP;
	State.StageIndex = Save->StageIndex;
	State.Levels = Save->UpgradeLevels;
	State.TotalClicks = Save->TotalClicks;
	Economy.SanitizeState();
	bReducedMotion = Save->bReducedMotion;
	DisplayMonitor = Save->DisplayMonitor;
	MasterVolume = FMath::Clamp(Save->MasterVolume, 0.f, 1.f);
	MusicVolume = FMath::Clamp(Save->MusicVolume, 0.f, 1.f);
	EffectsVolume = Save->bSoundEnabled ? FMath::Clamp(Save->EffectsVolume, 0.f, 1.f) : 0.f;
	UnlockedAchievements = Save->UnlockedAchievements;
	LongestOfflineSeconds = FMath::IsFinite(Save->LongestOfflineSeconds) ? FMath::Max(0.0, Save->LongestOfflineSeconds) : 0.0;

	// Offline earnings: measured from the last save, capped, granted once.
	// We save immediately afterwards so the same interval can never be claimed again.
	const FDateTime Now = FDateTime::UtcNow();
	const double Elapsed = (Now - Save->LastSaveUtc).GetTotalSeconds();
	double Counted = 0.0;
	const double Gain = Economy.ComputeOfflineGain(Elapsed, &Counted);
	if (Gain > 0.0)
	{
		Economy.Earn(Gain);
		PendingOfflineGain = Gain;
		PendingOfflineSeconds = Counted;
		LongestOfflineSeconds = FMath::Max(LongestOfflineSeconds, Counted);
		UE_LOG(LogEvolution, Log, TEXT("Offline for %.0fs (counted %.0fs): +%.0f EP"), Elapsed, Counted, Gain);
	}

	// Catch up on anything earned before achievements existed (or while offline). No toasts at load.
	CheckAchievements(false);
	SaveNow();
}

void UEvolutionGameSubsystem::SaveNow()
{
	if (!bDataValid)
	{
		return;
	}

	UEvolutionSaveGame* Save = Cast<UEvolutionSaveGame>(UGameplayStatics::CreateSaveGameObject(UEvolutionSaveGame::StaticClass()));
	if (!Save)
	{
		return;
	}

	const FEvoState& State = Economy.GetState();
	const FDateTime Now = FDateTime::UtcNow();
	Save->SaveVersion = UEvolutionSaveGame::CurrentVersion;
	Save->CurrentEP = State.CurrentEP;
	Save->LifetimeEP = State.LifetimeEP;
	Save->StageIndex = State.StageIndex;
	Save->UpgradeLevels = State.Levels;
	Save->TotalClicks = State.TotalClicks;
	Save->LastSaveUtc = Now;
	Save->LastOfflineClaimUtc = Now;
	Save->bReducedMotion = bReducedMotion;
	Save->DisplayMonitor = DisplayMonitor;
	Save->bSoundEnabled = EffectsVolume > 0.f;
	Save->MasterVolume = MasterVolume;
	Save->MusicVolume = MusicVolume;
	Save->EffectsVolume = EffectsVolume;
	Save->UnlockedAchievements = UnlockedAchievements;
	Save->LongestOfflineSeconds = LongestOfflineSeconds;

	if (!UGameplayStatics::SaveGameToSlot(Save, SaveSlotName, SaveUserIndex))
	{
		UE_LOG(LogEvolution, Warning, TEXT("Failed to write save slot %s"), *SaveSlotName);
	}
	AutosaveTimer = 0.0;
}

bool UEvolutionGameSubsystem::TickEconomy(float DeltaTime)
{
	if (!bDataValid)
	{
		return true;
	}

	const double Seconds = FMath::Clamp(static_cast<double>(DeltaTime), 0.0, MaxTickSeconds);
	Economy.Advance(Seconds);

	AchievementCheckTimer += Seconds;
	if (AchievementCheckTimer >= AchievementCheckSeconds)
	{
		AchievementCheckTimer = 0.0;
		CheckAchievements();
	}

	if (Steam.IsValid())
	{
		Steam->Tick(DeltaTime);
	}

	UpdateMusic();

	AutosaveTimer += Seconds;
	if (AutosaveTimer >= Data.Economy.AutosaveSeconds)
	{
		SaveNow();
	}
	return true; // keep ticking
}

double UEvolutionGameSubsystem::Click()
{
	if (!bDataValid)
	{
		return 0.0;
	}

	const double Rate = Data.Economy.MaxClicksPerSecond;
	if (Rate > 0.0)
	{
		// Allow a short burst (a quick double click) but never more than Rate clicks per second sustained,
		// so autoclickers can't outpace the economy.
		constexpr double BurstClicks = 3.0;
		const double Now = FPlatformTime::Seconds();
		if (LastClickTime <= 0.0)
		{
			ClickTokens = BurstClicks;
		}
		else
		{
			ClickTokens = FMath::Min(BurstClicks, ClickTokens + (Now - LastClickTime) * Rate);
		}
		LastClickTime = Now;
		if (ClickTokens < 1.0)
		{
			return 0.0;
		}
		ClickTokens -= 1.0;
	}
	return Economy.Click();
}

bool UEvolutionGameSubsystem::BuyUpgrade(const FString& UpgradeId)
{
	if (!bDataValid || !Economy.TryBuyUpgrade(UpgradeId))
	{
		return false;
	}
	OnUpgradeBought.Broadcast(UpgradeId);
	CheckAchievements();
	return true;
}

bool UEvolutionGameSubsystem::Evolve()
{
	if (!bDataValid || !Economy.TryEvolve())
	{
		return false;
	}
	SaveNow();
	PlayEvolveSound();
	OnStageChanged.Broadcast(Economy.GetState().StageIndex);
	CheckAchievements();
	return true;
}

void UEvolutionGameSubsystem::ResetProgress()
{
	if (!bDataValid)
	{
		return;
	}
	Economy.ResetState();
	PendingOfflineGain = 0.0;
	PendingOfflineSeconds = 0.0;
	SaveNow();
	OnProgressReset.Broadcast();
}

void UEvolutionGameSubsystem::QuitGame()
{
	SaveNow();
	UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	APlayerController* Controller = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
	UKismetSystemLibrary::QuitGame(World, Controller, EQuitPreference::Quit, false);
}

void UEvolutionGameSubsystem::SetReducedMotion(bool bEnabled)
{
	if (bReducedMotion != bEnabled)
	{
		bReducedMotion = bEnabled;
		SaveNow();
	}
}

void UEvolutionGameSubsystem::SetDisplayMonitor(int32 MonitorIndex)
{
	if (DisplayMonitor != MonitorIndex)
	{
		DisplayMonitor = MonitorIndex;
		SaveNow();
	}
}

void UEvolutionGameSubsystem::SetMasterVolume(float Value)
{
	MasterVolume = FMath::Clamp(Value, 0.f, 1.f);
	if (IsValid(EvolveAudio))
	{
		EvolveAudio->SetVolumeMultiplier(SliderToGain(MasterVolume) * SliderToGain(EffectsVolume));
	}
	UpdateMusic();
}

void UEvolutionGameSubsystem::SetMusicVolume(float Value)
{
	MusicVolume = FMath::Clamp(Value, 0.f, 1.f);
	UpdateMusic();
}

void UEvolutionGameSubsystem::SetEffectsVolume(float Value)
{
	EffectsVolume = FMath::Clamp(Value, 0.f, 1.f);
	if (IsValid(EvolveAudio))
	{
		EvolveAudio->SetVolumeMultiplier(SliderToGain(MasterVolume) * SliderToGain(EffectsVolume));
	}
}

void UEvolutionGameSubsystem::LoadSounds()
{
	const FString AudioDir = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Audio"));

	const FString EvolvePath = FPaths::Combine(AudioDir, TEXT("evolve.wav"));
	if (!ReadPcmWav(EvolvePath, EvolvePCM, EvolveSampleRate, EvolveChannels))
	{
		EvolvePCM.Reset();
		UE_LOG(LogEvolution, Warning, TEXT("Evolve sound missing or not 16-bit PCM WAV: %s"), *EvolvePath);
	}

	// Music: every .wav in Audio/Music, in name order, played back to back. All parts must share one format.
	const FString MusicDir = FPaths::Combine(AudioDir, TEXT("Music"));
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(MusicDir, TEXT("*.wav")), true, false);
	Files.Sort();
	for (const FString& Name : Files)
	{
		TArray<uint8> Part;
		int32 Rate = 0;
		int32 Channels = 0;
		const FString Path = FPaths::Combine(MusicDir, Name);
		if (!ReadPcmWav(Path, Part, Rate, Channels))
		{
			UE_LOG(LogEvolution, Warning, TEXT("Music file is not 16-bit PCM WAV, skipped: %s"), *Path);
			continue;
		}
		if (MusicPCM.Num() > 0 && (Rate != MusicSampleRate || Channels != MusicChannels))
		{
			UE_LOG(LogEvolution, Warning, TEXT("Music file %s has a different format (%d Hz, %d ch) from the first track; skipped."), *Name, Rate, Channels);
			continue;
		}
		MusicSampleRate = Rate;
		MusicChannels = Channels;
		MusicPCM.Append(Part);
	}
	if (MusicPCM.Num() > 0)
	{
		UE_LOG(LogEvolution, Log, TEXT("Loaded %.0f s of music from %d file(s)."),
			static_cast<double>(MusicPCM.Num()) / (MusicSampleRate * MusicChannels * 2), Files.Num());
	}
}

void UEvolutionGameSubsystem::UpdateMusic()
{
	if (MusicPCM.Num() == 0)
	{
		return;
	}
	const float Gain = SliderToGain(MasterVolume) * SliderToGain(MusicVolume);

	UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	const double Now = FPlatformTime::Seconds();

	// Safety net: if the engine stopped the music anyway (e.g. an audio device change), start a fresh one.
	if (IsValid(MusicAudio) && bMusicStarted && !MusicAudio->IsPlaying())
	{
		if (MusicStoppedSince <= 0.0)
		{
			MusicStoppedSince = Now;
		}
		else if (Now - MusicStoppedSince > 1.0)
		{
			UE_LOG(LogEvolution, Log, TEXT("Music stopped unexpectedly; restarting it."));
			MusicAudio->Stop();
			MusicAudio = nullptr;
		}
	}
	else
	{
		MusicStoppedSince = 0.0;
	}

	if (!IsValid(MusicAudio) || MusicAudio->GetWorld() != World)
	{
		// A procedural wave must only ever feed one playing sound, so each new audio component gets its own wave.
		if (IsValid(MusicAudio))
		{
			MusicAudio->Stop();
		}
		MusicWave = NewObject<USoundWaveProcedural>(this);
		MusicWave->SetSampleRate(MusicSampleRate);
		MusicWave->NumChannels = MusicChannels;
		MusicWave->Duration = INDEFINITELY_LOOPING_DURATION;
		MusicWave->SoundGroup = SOUNDGROUP_Default;
		MusicWave->bLooping = false;
		// Keep playing while silent (fade-in start, or the Music slider at Off) instead of being culled.
		MusicWave->VirtualizationMode = EVirtualizationMode::PlayWhenSilent;

		MusicAudio = UGameplayStatics::CreateSound2D(World, MusicWave, 1.f, 1.f, 0.f, nullptr, true, false);
		MusicReadPos = 0;
		bMusicStarted = false;
		MusicStoppedSince = 0.0;
		if (!MusicAudio)
		{
			return;
		}
	}

	// Our own 2-second fade-in (the engine's FadeIn starts at true silence, which can get the sound culled).
	const float Fade = bMusicStarted ? FMath::Clamp(static_cast<float>((Now - MusicStartTime) / 2.0), 0.02f, 1.f) : 0.02f;
	MusicAudio->SetVolumeMultiplier(FMath::Max(Gain * Fade, 0.001f));

	// Keep a few seconds queued, wrapping to the start so the track loops without a gap.
	const int32 FrameBytes = MusicChannels * 2;
	const int32 BufferBytes = static_cast<int32>(MusicBufferSeconds * MusicSampleRate) * FrameBytes;
	const int32 ChunkBytes = static_cast<int32>(MusicChunkSeconds * MusicSampleRate) * FrameBytes;
	int32 Guard = 8;
	while (MusicWave->GetAvailableAudioByteCount() < BufferBytes && Guard-- > 0)
	{
		const int64 Remaining = MusicPCM.Num() - MusicReadPos;
		const int32 Bytes = static_cast<int32>(FMath::Min<int64>(ChunkBytes, Remaining));
		MusicWave->QueueAudio(MusicPCM.GetData() + MusicReadPos, Bytes);
		MusicReadPos += Bytes;
		if (MusicReadPos >= MusicPCM.Num())
		{
			MusicReadPos = 0;
		}
	}

	// Start exactly once per component. (IsPlaying() can read false for a frame after starting, and
	// restarting would make two sounds pull from the same wave at once.)
	if (!bMusicStarted)
	{
		bMusicStarted = true;
		MusicStartTime = Now;
		MusicAudio->Play();
		UE_LOG(LogEvolution, Log, TEXT("Music started."));
	}
}

void UEvolutionGameSubsystem::PlayEvolveSound()
{
	const float Gain = SliderToGain(MasterVolume) * SliderToGain(EffectsVolume);
	if (Gain <= 0.f || EvolvePCM.Num() == 0)
	{
		return;
	}
	UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	// Cut off a chime that is still playing, then play a fresh one. Each chime gets its own procedural wave,
	// because a procedural wave must never feed two playing sounds at once.
	if (IsValid(EvolveAudio))
	{
		EvolveAudio->Stop();
	}
	EvolveWave = NewObject<USoundWaveProcedural>(this);
	EvolveWave->SetSampleRate(EvolveSampleRate);
	EvolveWave->NumChannels = EvolveChannels;
	EvolveWave->Duration = static_cast<float>(EvolvePCM.Num()) / static_cast<float>(EvolveSampleRate * EvolveChannels * 2);
	EvolveWave->SoundGroup = SOUNDGROUP_Default;
	EvolveWave->bLooping = false;
	EvolveWave->QueueAudio(EvolvePCM.GetData(), EvolvePCM.Num());

	// Auto-destroys when finished.
	EvolveAudio = UGameplayStatics::CreateSound2D(World, EvolveWave, Gain, 1.f, 0.f, nullptr, false, true);
	if (EvolveAudio)
	{
		EvolveAudio->Play();
	}
}

void UEvolutionGameSubsystem::ClearOfflineReport()
{
	PendingOfflineGain = 0.0;
	PendingOfflineSeconds = 0.0;
}

// ---------------------------------------------------------------------------
// Achievements
// ---------------------------------------------------------------------------

FDateTime UEvolutionGameSubsystem::GetAchievementUnlockTime(const FString& Id) const
{
	const FDateTime* Time = UnlockedAchievements.Find(Id);
	return Time ? *Time : FDateTime(0);
}

FEvoAchievementProgress UEvolutionGameSubsystem::GetAchievementProgress(const FEvoAchievementDef& Def) const
{
	if (!bDataValid)
	{
		return FEvoAchievementProgress();
	}
	return EvoAchievements::Evaluate(Def, Economy, LongestOfflineSeconds);
}

int32 UEvolutionGameSubsystem::GetUnlockedAchievementCount() const
{
	int32 Count = 0;
	for (const FEvoAchievementDef& Def : Data.Achievements)
	{
		if (UnlockedAchievements.Contains(Def.Id))
		{
			++Count;
		}
	}
	return Count;
}

const FEvoAchievementDef* UEvolutionGameSubsystem::FindAchievement(const FString& Id) const
{
	return Data.Achievements.FindByPredicate([&Id](const FEvoAchievementDef& Def) { return Def.Id == Id; });
}

bool UEvolutionGameSubsystem::IsSteamActive() const
{
	return Steam.IsValid() && Steam->IsActive();
}

bool UEvolutionGameSubsystem::CheckAchievements(bool bAnnounce)
{
	if (!bDataValid)
	{
		return false;
	}

	TArray<FString> NewlyUnlocked;
	const FDateTime Now = FDateTime::UtcNow();
	for (const FEvoAchievementDef& Def : Data.Achievements)
	{
		if (UnlockedAchievements.Contains(Def.Id))
		{
			continue;
		}
		if (EvoAchievements::Evaluate(Def, Economy, LongestOfflineSeconds).IsComplete())
		{
			UnlockedAchievements.Add(Def.Id, Now);
			NewlyUnlocked.Add(Def.Id);
			UE_LOG(LogEvolution, Log, TEXT("Achievement unlocked: %s (%s)"), *Def.Name, *Def.Id);
		}
	}

	if (NewlyUnlocked.Num() == 0)
	{
		return false;
	}

	for (const FString& Id : NewlyUnlocked)
	{
		if (Steam.IsValid())
		{
			Steam->Unlock(Id);
		}
		if (bAnnounce)
		{
			OnAchievementUnlocked.Broadcast(Id);
		}
	}
	SaveNow();
	return true;
}

void UEvolutionGameSubsystem::InitSteam()
{
	Steam = MakeShared<FEvoSteam>();
	if (!Steam->Init())
	{
		return;
	}

	// Steam is the record across PCs: adopt unlocks from Steam, and send Steam any it is missing
	// (earned while Steam was not running).
	const FDateTime Now = FDateTime::UtcNow();
	bool bChanged = false;
	for (const FEvoAchievementDef& Def : Data.Achievements)
	{
		const bool bLocal = UnlockedAchievements.Contains(Def.Id);
		if (Steam->IsUnlockedOnSteam(Def.Id))
		{
			if (!bLocal)
			{
				UnlockedAchievements.Add(Def.Id, Now);
				bChanged = true;
			}
		}
		else if (bLocal)
		{
			Steam->Unlock(Def.Id);
		}
	}
	Steam->Flush();
	if (bChanged)
	{
		SaveNow();
	}
}
