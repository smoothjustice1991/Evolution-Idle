#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "EvoGameData.h"
#include "EvoEconomy.h"
#include "EvolutionGameSubsystem.generated.h"

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

	FEvoGameData Data;
	FEvoEconomy Economy;
	bool bDataValid = false;
	FString DataError;

	FTSTicker::FDelegateHandle TickHandle;
	double AutosaveTimer = 0.0;
	bool bReducedMotion = false;

	double PendingOfflineGain = 0.0;
	double PendingOfflineSeconds = 0.0;
};
