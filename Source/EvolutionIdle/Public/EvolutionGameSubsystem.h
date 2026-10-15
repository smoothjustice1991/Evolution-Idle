#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "EvoGameData.h"
#include "EvoEconomy.h"
#include "EvolutionGameSubsystem.generated.h"

class USoundWaveProcedural;
class UAudioComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnEvoStageChanged, int32 /*NewStageIndex*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnEvoUpgradeBought, const FString& /*UpgradeId*/);
DECLARE_MULTICAST_DELEGATE(FOnEvoProgressReset);

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

	bool GetSoundEnabled() const { return bSoundEnabled; }
	void SetSoundEnabled(bool bEnabled);

	/** Monitor (0-based) the player chose for the game window; -1 = not chosen yet. */
	int32 GetDisplayMonitor() const { return DisplayMonitor; }
	void SetDisplayMonitor(int32 MonitorIndex);

	// ---- Offline report (shown once by the HUD) ----
	double GetPendingOfflineGain() const { return PendingOfflineGain; }
	double GetPendingOfflineSeconds() const { return PendingOfflineSeconds; }
	void ClearOfflineReport();

	FOnEvoStageChanged OnStageChanged;
	FOnEvoUpgradeBought OnUpgradeBought;
	FOnEvoProgressReset OnProgressReset;

	static const FString SaveSlotName;

private:
	bool TickEconomy(float DeltaTime);
	void LoadGameData();
	void LoadProgress();

	/** Loads Content/Audio/evolve.wav (16-bit PCM) into memory. */
	void LoadSounds();
	void PlayEvolveSound();

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
	bool bSoundEnabled = true;

	// Evolve sound: PCM kept in memory and replayed through one reusable audio component.
	TArray<uint8> EvolvePCM;
	int32 EvolveSampleRate = 0;
	int32 EvolveChannels = 0;

	UPROPERTY()
	TObjectPtr<USoundWaveProcedural> EvolveWave;

	UPROPERTY()
	TObjectPtr<UAudioComponent> EvolveAudio;
	int32 DisplayMonitor = -1;

	double PendingOfflineGain = 0.0;
	double PendingOfflineSeconds = 0.0;
};
