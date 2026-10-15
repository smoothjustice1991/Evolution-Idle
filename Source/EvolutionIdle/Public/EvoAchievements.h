#pragma once

#include "CoreMinimal.h"
#include "EvoGameData.h"

class FEvoEconomy;

/** How far the player is toward one achievement. */
struct FEvoAchievementProgress
{
	double Current = 0.0;
	double Target = 1.0;

	bool IsComplete() const { return Target > 0.0 && Current >= Target; }
	float GetFraction() const { return Target > 0.0 ? static_cast<float>(FMath::Clamp(Current / Target, 0.0, 1.0)) : 0.f; }
};

/** Pure achievement rules (no UObjects), so they can be unit tested. */
namespace EvoAchievements
{
	/**
	 * Progress toward an achievement for the current game state.
	 * LongestOfflineSeconds is the longest offline period the player has been paid for.
	 */
	EVOLUTIONIDLE_API FEvoAchievementProgress Evaluate(const FEvoAchievementDef& Def, const FEvoEconomy& Economy, double LongestOfflineSeconds);

	/** Total auto-clicker units owned (levels of all production upgrades). */
	EVOLUTIONIDLE_API int32 CountAutoClickers(const FEvoEconomy& Economy);

	/** Number of different upgrades the player owns at least one level of. */
	EVOLUTIONIDLE_API int32 CountOwnedUpgradeTypes(const FEvoEconomy& Economy);

	/** Id of the last stage in an era, or INDEX_NONE if the era has no stages. */
	EVOLUTIONIDLE_API int32 GetLastStageOfEra(const FEvoGameData& Data, int32 EraId);
}
