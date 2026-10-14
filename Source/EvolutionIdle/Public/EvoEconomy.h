#pragma once

#include "CoreMinimal.h"
#include "EvoGameData.h"

/** Mutable player progress. Everything here is persisted by the save game. */
struct EVOLUTIONIDLE_API FEvoState
{
	double CurrentEP = 0.0;
	double LifetimeEP = 0.0;
	int32 StageIndex = 0;            // index into FEvoGameData::Stages
	TMap<FString, int32> Levels;     // upgrade id -> owned count / level
	int64 TotalClicks = 0;
};

/**
 * Pure economy rules (no UObjects, no world), so they can be unit tested.
 *
 * Bonus order (documented per GDD Appendix A implementation note):
 *   GlobalMultiplier = (1 + sum(multiplier levels * amount)) * (1 + sum(productionBonus of reached stages))
 *   EP per click     = (BaseClickPower + sum(click levels * amount)) * GlobalMultiplier
 *   EP per second    = sum(production levels * amount) * GlobalMultiplier
 *   Upgrade cost     = floor(BaseCost * r^level * EraCostMultiplier)
 */
class EVOLUTIONIDLE_API FEvoEconomy
{
public:
	/** Data must outlive this object. */
	void Init(const FEvoGameData* InData);

	const FEvoGameData& GetData() const;
	FEvoState& GetState() { return State; }
	const FEvoState& GetState() const { return State; }
	void ResetState();

	// ---- Upgrades ----
	const FEvoUpgradeDef* FindUpgrade(const FString& Id) const;
	int32 GetLevel(const FString& Id) const;
	double GetUpgradeCost(const FEvoUpgradeDef& Upgrade) const;
	bool IsUnlocked(const FEvoUpgradeDef& Upgrade) const;
	bool IsMaxed(const FEvoUpgradeDef& Upgrade) const;
	bool CanBuyUpgrade(const FEvoUpgradeDef& Upgrade) const;

	/** Additional EP/sec the next level would add (production and multiplier upgrades). */
	double GetMarginalEPPerSecond(const FEvoUpgradeDef& Upgrade) const;

	/** Additional EP per click the next level would add (click and multiplier upgrades). */
	double GetMarginalClickValue(const FEvoUpgradeDef& Upgrade) const;

	/** Seconds of passive production needed to earn back the cost; negative if it adds no EP/sec. */
	double GetPaybackSeconds(const FEvoUpgradeDef& Upgrade) const;

	// ---- Production ----
	double GetUpgradeMultiplier() const;
	double GetStageBonusMultiplier() const;
	double GetGlobalMultiplier() const;
	double GetClickValue() const;
	double GetBaseEPPerSecond() const;
	double GetEPPerSecond() const;

	// ---- Stages ----
	const FEvoStageDef* GetCurrentStage() const;
	const FEvoStageDef* GetNextStage() const;
	const FEvoEraDef* GetCurrentEra() const;
	bool IsCampaignComplete() const;
	bool CanEvolve() const;

	// ---- Actions ----
	/** Adds EP to both current and lifetime totals. Ignores non-positive or non-finite amounts. */
	void Earn(double Amount);

	/** Manual click. Returns the EP granted. */
	double Click();

	/** Passive production for a time slice. Returns the EP granted. */
	double Advance(double Seconds);

	/** Deducts the displayed cost and applies the upgrade once. Returns false (and changes nothing) if not allowed. */
	bool TryBuyUpgrade(const FString& Id, double* OutCostPaid = nullptr);

	/** Deducts the next stage cost and advances one stage. Returns false (and changes nothing) if not allowed. */
	bool TryEvolve(double* OutCostPaid = nullptr);

	/** EP earned while offline for the given elapsed seconds, applying cap and efficiency. Does not grant it. */
	double ComputeOfflineGain(double ElapsedSeconds, double* OutCountedSeconds = nullptr) const;

	/** Clamps loaded state into valid ranges (unknown upgrades removed, negative/non-finite values fixed). */
	void SanitizeState();

private:
	double SumLevels(EEvoEffect Effect, const FString* ExtraLevelFor = nullptr) const;

	const FEvoGameData* Data = nullptr;
	FEvoState State;
};
