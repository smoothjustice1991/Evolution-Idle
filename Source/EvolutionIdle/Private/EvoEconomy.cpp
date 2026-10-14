#include "EvoEconomy.h"

namespace
{
	const FEvoGameData& GetEmptyData()
	{
		static const FEvoGameData Empty;
		return Empty;
	}

	bool IsValidAmount(double Value)
	{
		return FMath::IsFinite(Value) && Value > 0.0;
	}
}

void FEvoEconomy::Init(const FEvoGameData* InData)
{
	Data = InData;
	ResetState();
}

const FEvoGameData& FEvoEconomy::GetData() const
{
	return Data ? *Data : GetEmptyData();
}

void FEvoEconomy::ResetState()
{
	State = FEvoState();
}

// ---------------------------------------------------------------------------
// Upgrades
// ---------------------------------------------------------------------------

const FEvoUpgradeDef* FEvoEconomy::FindUpgrade(const FString& Id) const
{
	return GetData().Upgrades.FindByPredicate([&Id](const FEvoUpgradeDef& Upgrade) { return Upgrade.Id == Id; });
}

int32 FEvoEconomy::GetLevel(const FString& Id) const
{
	const int32* Level = State.Levels.Find(Id);
	return Level ? *Level : 0;
}

double FEvoEconomy::GetUpgradeCost(const FEvoUpgradeDef& Upgrade) const
{
	const FEvoGameData& D = GetData();
	const double Scaling = Upgrade.CostScaling > 0.0 ? Upgrade.CostScaling : D.Economy.DefaultCostScaling;
	const double Raw = Upgrade.BaseCost * FMath::Pow(Scaling, static_cast<double>(GetLevel(Upgrade.Id))) * D.GetEraCostMultiplier(Upgrade.Era);
	return FMath::IsFinite(Raw) ? FMath::FloorToDouble(Raw) : TNumericLimits<double>::Max();
}

bool FEvoEconomy::IsUnlocked(const FEvoUpgradeDef& Upgrade) const
{
	const FEvoStageDef* Stage = GetCurrentStage();
	return Stage && Upgrade.UnlockStage <= Stage->Id;
}

bool FEvoEconomy::IsMaxed(const FEvoUpgradeDef& Upgrade) const
{
	return Upgrade.MaxLevel > 0 && GetLevel(Upgrade.Id) >= Upgrade.MaxLevel;
}

bool FEvoEconomy::CanBuyUpgrade(const FEvoUpgradeDef& Upgrade) const
{
	return IsUnlocked(Upgrade)
		&& !IsMaxed(Upgrade)
		&& Upgrade.GetEffect() != EEvoEffect::Unknown
		&& GetUpgradeCost(Upgrade) <= State.CurrentEP;
}

double FEvoEconomy::SumLevels(EEvoEffect Effect, const FString* ExtraLevelFor) const
{
	double Total = 0.0;
	for (const FEvoUpgradeDef& Upgrade : GetData().Upgrades)
	{
		if (Upgrade.GetEffect() != Effect)
		{
			continue;
		}
		int32 Level = GetLevel(Upgrade.Id);
		if (ExtraLevelFor && *ExtraLevelFor == Upgrade.Id)
		{
			++Level;
		}
		Total += static_cast<double>(Level) * Upgrade.Amount;
	}
	return Total;
}

double FEvoEconomy::GetMarginalEPPerSecond(const FEvoUpgradeDef& Upgrade) const
{
	const double StageMult = GetStageBonusMultiplier();
	const double After = SumLevels(EEvoEffect::Production, &Upgrade.Id)
		* (1.0 + SumLevels(EEvoEffect::Multiplier, &Upgrade.Id)) * StageMult;
	return FMath::Max(0.0, After - GetEPPerSecond());
}

double FEvoEconomy::GetMarginalClickValue(const FEvoUpgradeDef& Upgrade) const
{
	const double StageMult = GetStageBonusMultiplier();
	const double After = (GetData().Economy.BaseClickPower + SumLevels(EEvoEffect::ClickPower, &Upgrade.Id))
		* (1.0 + SumLevels(EEvoEffect::Multiplier, &Upgrade.Id)) * StageMult;
	return FMath::Max(0.0, After - GetClickValue());
}

double FEvoEconomy::GetPaybackSeconds(const FEvoUpgradeDef& Upgrade) const
{
	const double Gain = GetMarginalEPPerSecond(Upgrade);
	return Gain > 0.0 ? GetUpgradeCost(Upgrade) / Gain : -1.0;
}

// ---------------------------------------------------------------------------
// Production
// ---------------------------------------------------------------------------

double FEvoEconomy::GetUpgradeMultiplier() const
{
	return 1.0 + SumLevels(EEvoEffect::Multiplier);
}

double FEvoEconomy::GetStageBonusMultiplier() const
{
	double Bonus = 0.0;
	const TArray<FEvoStageDef>& Stages = GetData().Stages;
	for (int32 Index = 0; Index <= State.StageIndex && Index < Stages.Num(); ++Index)
	{
		Bonus += Stages[Index].ProductionBonus;
	}
	return 1.0 + Bonus;
}

double FEvoEconomy::GetGlobalMultiplier() const
{
	return GetUpgradeMultiplier() * GetStageBonusMultiplier();
}

double FEvoEconomy::GetClickValue() const
{
	return (GetData().Economy.BaseClickPower + SumLevels(EEvoEffect::ClickPower)) * GetGlobalMultiplier();
}

double FEvoEconomy::GetBaseEPPerSecond() const
{
	return SumLevels(EEvoEffect::Production);
}

double FEvoEconomy::GetEPPerSecond() const
{
	return GetBaseEPPerSecond() * GetGlobalMultiplier();
}

// ---------------------------------------------------------------------------
// Stages
// ---------------------------------------------------------------------------

const FEvoStageDef* FEvoEconomy::GetCurrentStage() const
{
	const TArray<FEvoStageDef>& Stages = GetData().Stages;
	return Stages.IsValidIndex(State.StageIndex) ? &Stages[State.StageIndex] : nullptr;
}

const FEvoStageDef* FEvoEconomy::GetNextStage() const
{
	const TArray<FEvoStageDef>& Stages = GetData().Stages;
	return Stages.IsValidIndex(State.StageIndex + 1) ? &Stages[State.StageIndex + 1] : nullptr;
}

const FEvoEraDef* FEvoEconomy::GetCurrentEra() const
{
	const FEvoStageDef* Stage = GetCurrentStage();
	return Stage ? GetData().FindEra(Stage->Era) : nullptr;
}

bool FEvoEconomy::IsCampaignComplete() const
{
	return GetData().Stages.Num() > 0 && GetNextStage() == nullptr;
}

bool FEvoEconomy::CanEvolve() const
{
	const FEvoStageDef* Next = GetNextStage();
	return Next && Next->Cost <= State.CurrentEP;
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

void FEvoEconomy::Earn(double Amount)
{
	if (!IsValidAmount(Amount))
	{
		return;
	}
	State.CurrentEP += Amount;
	State.LifetimeEP += Amount;
}

double FEvoEconomy::Click()
{
	const double Gain = GetClickValue();
	Earn(Gain);
	++State.TotalClicks;
	return Gain;
}

double FEvoEconomy::Advance(double Seconds)
{
	if (!IsValidAmount(Seconds))
	{
		return 0.0;
	}
	const double Gain = GetEPPerSecond() * Seconds;
	Earn(Gain);
	return IsValidAmount(Gain) ? Gain : 0.0;
}

bool FEvoEconomy::TryBuyUpgrade(const FString& Id, double* OutCostPaid)
{
	const FEvoUpgradeDef* Upgrade = FindUpgrade(Id);
	if (!Upgrade || !CanBuyUpgrade(*Upgrade))
	{
		return false;
	}
	const double Cost = GetUpgradeCost(*Upgrade);
	State.CurrentEP = FMath::Max(0.0, State.CurrentEP - Cost);
	State.Levels.FindOrAdd(Id) += 1;
	if (OutCostPaid)
	{
		*OutCostPaid = Cost;
	}
	return true;
}

bool FEvoEconomy::TryEvolve(double* OutCostPaid)
{
	if (!CanEvolve())
	{
		return false;
	}
	const double Cost = GetNextStage()->Cost;
	State.CurrentEP = FMath::Max(0.0, State.CurrentEP - Cost);
	++State.StageIndex;
	if (OutCostPaid)
	{
		*OutCostPaid = Cost;
	}
	return true;
}

double FEvoEconomy::ComputeOfflineGain(double ElapsedSeconds, double* OutCountedSeconds) const
{
	const FEvoEconomyConfig& Config = GetData().Economy;
	const double Counted = FMath::IsFinite(ElapsedSeconds)
		? FMath::Clamp(ElapsedSeconds, 0.0, Config.OfflineCapHours * 3600.0)
		: 0.0;
	if (OutCountedSeconds)
	{
		*OutCountedSeconds = Counted;
	}
	const double Gain = GetEPPerSecond() * Counted * Config.OfflineEfficiency;
	return IsValidAmount(Gain) ? Gain : 0.0;
}

void FEvoEconomy::SanitizeState()
{
	if (!FMath::IsFinite(State.CurrentEP) || State.CurrentEP < 0.0)
	{
		State.CurrentEP = 0.0;
	}
	if (!FMath::IsFinite(State.LifetimeEP) || State.LifetimeEP < State.CurrentEP)
	{
		State.LifetimeEP = State.CurrentEP;
	}
	State.StageIndex = FMath::Clamp(State.StageIndex, 0, FMath::Max(0, GetData().Stages.Num() - 1));
	State.TotalClicks = FMath::Max<int64>(0, State.TotalClicks);

	for (auto It = State.Levels.CreateIterator(); It; ++It)
	{
		const FEvoUpgradeDef* Upgrade = FindUpgrade(It.Key());
		if (!Upgrade || It.Value() <= 0)
		{
			It.RemoveCurrent();
		}
		else if (Upgrade->MaxLevel > 0)
		{
			It.Value() = FMath::Min(It.Value(), Upgrade->MaxLevel);
		}
	}
}
