#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "EvoGameData.h"
#include "EvoEconomy.h"
#include "EvoAchievements.h"
#include "EvolutionGameSubsystem.generated.h"

class USoundWaveProcedural;
class UAudioComponent;
class FEvoSteam;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnEvoStageChanged, int32 /*NewStageIndex*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnEvoUpgradeBought, const FString& /*UpgradeId*/);
DECLARE_MULTICAST_DELEGATE(FOnEvoProgressReset);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnEvoAchievementUnlocked, const FString& /*AchievementId*/);

/**
 * Owns game data, the economy, production ticking, saving and offline earnings.
 * Lives for the whole game session (GameState / EvolutionManager / ProductionManager / SaveManager in the GDD).
 */
UCLASS()
class EVOLUTIONIDLE_API UEvolutionGameSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	bool HasGameData() const { return bDataValid; }
	const FString& GetDataError() const { return DataError; }

	const FEvoEconomy& GetEconomy() const { return Economy; }

	// ---- Player actions ----
	/** Manual click. Returns the EP granted, or 0 if the click came faster than the click-rate cap allows. */
	double Click();
	bool BuyUpgrade(const FString& UpgradeId);
	bool Evolve();

	// ---- Persistence ----
	void SaveNow();
	void ResetProgress();

	/** Saves, then closes the game (ends Play-in-Editor when run from the editor). */
	void QuitGame();

	// ---- Settings ----
	bool GetReducedMotion() const { return bReducedMotion; }
	void SetReducedMotion(bool bEnabled);

	// Audio levels, 0..1 as shown on the settings sliders. Changes apply immediately; call SaveNow() to persist.
	float GetMasterVolume() const { return MasterVolume; }
	float GetMusicVolume() const { return MusicVolume; }
	float GetEffectsVolume() const { return EffectsVolume; }
	void SetMasterVolume(float Value);
	void SetMusicVolume(float Value);
	void SetEffectsVolume(float Value);

	/** Monitor (0-based) the player chose for the game window; -1 = not chosen yet. */
	int32 GetDisplayMonitor() const { return DisplayMonitor; }
	void SetDisplayMonitor(int32 MonitorIndex);

	// ---- Achievements ----
	const TArray<FEvoAchievementDef>& GetAchievements() const { return Data.Achievements; }
	bool IsAchievementUnlocked(const FString& Id) const { return UnlockedAchievements.Contains(Id); }
	/** UTC unlock time, or FDateTime(0) if locked. */
	FDateTime GetAchievementUnlockTime(const FString& Id) const;
	FEvoAchievementProgress GetAchievementProgress(const FEvoAchievementDef& Def) const;
	int32 GetUnlockedAchievementCount() const;
	const FEvoAchievementDef* FindAchievement(const FString& Id) const;
	/** True when unlocks are being mirrored to Steam right now. */
	bool IsSteamActive() const;

	// ---- Offline report (shown once by the HUD) ----
	double GetPendingOfflineGain() const { return PendingOfflineGain; }
	double GetPendingOfflineSeconds() const { return PendingOfflineSeconds; }
	void ClearOfflineReport();

	FOnEvoStageChanged OnStageChanged;
	FOnEvoUpgradeBought OnUpgradeBought;
	FOnEvoProgressReset OnProgressReset;
	FOnEvoAchievementUnlocked OnAchievementUnlocked;

	static const FString SaveSlotName;

private:
	bool TickEconomy(float DeltaTime);
	void LoadGameData();
	void LoadProgress();

	/** Unlocks every achievement whose condition is now met. Returns true if any unlocked. */
	bool CheckAchievements(bool bAnnounce = true);

	/** Starts Steam and makes Steam and the save agree on what is unlocked. */
	void InitSteam();

	/** Loads Content/Audio/evolve.wav and the music in Content/Audio/Music (16-bit PCM WAV) into memory. */
	void LoadSounds();
	void PlayEvolveSound();

	/** Starts the music once a world exists, keeps its stream fed (looping), and applies the volume. */
	void UpdateMusic();

	/** Slider position (0..1) to gain; squared so the slider feels even to the ear. */
	static float SliderToGain(float Value) { return Value * Value; }

	FEvoGameData Data;
	FEvoEconomy Economy;
	bool bDataValid = false;
	FString DataError;

	FTSTicker::FDelegateHandle TickHandle;
	double AutosaveTimer = 0.0;

	// Click-rate cap (token bucket): refills at MaxClicksPerSecond, holds a few clicks of burst.
	double ClickTokens = 0.0;
	double LastClickTime = 0.0;
	bool bReducedMotion = false;
	float MasterVolume = 1.f;
	float MusicVolume = 0.6f;
	float EffectsVolume = 1.f;

	// Evolve sound: PCM kept in memory; each play gets a fresh procedural wave and audio component.
	TArray<uint8> EvolvePCM;
	int32 EvolveSampleRate = 0;
	int32 EvolveChannels = 0;

	UPROPERTY()
	TObjectPtr<USoundWaveProcedural> EvolveWave;

	UPROPERTY()
	TObjectPtr<UAudioComponent> EvolveAudio;

	// Background music: every WAV in Content/Audio/Music, joined in name order and looped.
	// It is streamed into a procedural wave a few seconds ahead of playback.
	TArray<uint8> MusicPCM;
	int32 MusicSampleRate = 0;
	int32 MusicChannels = 0;
	int64 MusicReadPos = 0;
	bool bMusicStarted = false;
	double MusicStartTime = 0.0;
	double MusicStoppedSince = 0.0;

	UPROPERTY()
	TObjectPtr<USoundWaveProcedural> MusicWave;

	UPROPERTY()
	TObjectPtr<UAudioComponent> MusicAudio;
	int32 DisplayMonitor = -1;

	TMap<FString, FDateTime> UnlockedAchievements;
	double LongestOfflineSeconds = 0.0;
	double AchievementCheckTimer = 0.0;
	TSharedPtr<FEvoSteam> Steam;

	double PendingOfflineGain = 0.0;
	double PendingOfflineSeconds = 0.0;
};
