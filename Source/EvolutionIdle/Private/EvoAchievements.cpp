#include "EvoAchievements.h"
#include "EvoEconomy.h"

namespace EvoAchievements
{
	int32 CountAutoClickers(const FEvoEconomy& Economy)
	{
		int32 Count = 0;
		for (const FEvoUpgradeDef& Upgrade : Economy.GetData().Upgrades)
		{
			if (Upgrade.GetEffect() == EEvoEffect::Production)
			{
				Count += FMath::Max(0, Economy.GetLevel(Upgrade.Id));
			}
		}
		return Count;
	}

	int32 CountOwnedUpgradeTypes(const FEvoEconomy& Economy)
	{
		int32 Count = 0;
		for (const FEvoUpgradeDef& Upgrade : Economy.GetData().Upgrades)
		{
			if (Economy.GetLevel(Upgrade.Id) > 0)
			{
				++Count;
			}
		}
		return Count;
	}

	int32 GetLastStageOfEra(const FEvoGameData& Data, int32 EraId)
	{
		int32 Last = INDEX_NONE;
		for (const FEvoStageDef& Stage : Data.Stages)
		{
			if (Stage.Era == EraId)
			{
				Last = FMath::Max(Last, Stage.Id);
			}
		}
		return Last;
	}

	FEvoAchievementProgress Evaluate(const FEvoAchievementDef& Def, const FEvoEconomy& Economy, double LongestOfflineSeconds)
	{
		FEvoAchievementProgress Progress;
		const FEvoGameData& Data = Economy.GetData();
		const FEvoStageDef* Stage = Economy.GetCurrentStage();
		const double StageId = Stage ? static_cast<double>(Stage->Id) : 0.0;

		switch (Def.GetType())
		{
		case EEvoAchievementType::Stage:
			Progress.Current = StageId;
			Progress.Target = Def.Target;
			break;

		case EEvoAchievementType::EraComplete:
		{
			const int32 LastStage = GetLastStageOfEra(Data, FMath::RoundToInt32(Def.Target));
			Progress.Current = StageId;
			Progress.Target = LastStage == INDEX_NONE ? 0.0 : static_cast<double>(LastStage); // 0 = can never complete
			break;
		}

		case EEvoAchievementType::Clicks:
			Progress.Current = static_cast<double>(Economy.GetState().TotalClicks);
			Progress.Target = Def.Target;
			break;

		case EEvoAchievementType::AutoClickers:
			Progress.Current = CountAutoClickers(Economy);
			Progress.Target = Def.Target;
			break;

		case EEvoAchievementType::AllUpgrades:
			Progress.Current = CountOwnedUpgradeTypes(Economy);
			Progress.Target = Data.Upgrades.Num();
			break;

		case EEvoAchievementType::OfflineFull:
			Progress.Current = FMath::Max(0.0, LongestOfflineSeconds);
			Progress.Target = Data.Economy.OfflineCapHours * 3600.0;
			break;

		default:
			Progress.Current = 0.0;
			Progress.Target = 0.0;
			break;
		}

		if (!FMath::IsFinite(Progress.Current))
		{
			Progress.Current = 0.0;
		}
		if (!FMath::IsFinite(Progress.Target) || Progress.Target < 0.0)
		{
			Progress.Target = 0.0;
		}
		return Progress;
	}
}
