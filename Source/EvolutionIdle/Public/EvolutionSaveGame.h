#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "EvolutionSaveGame.generated.h"

UCLASS()
class EVOLUTIONIDLE_API UEvolutionSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentVersion = 1;

	UPROPERTY() int32 SaveVersion = CurrentVersion;

	UPROPERTY() double CurrentEP = 0.0;
	UPROPERTY() double LifetimeEP = 0.0;
	UPROPERTY() int32 StageIndex = 0;
	UPROPERTY() TMap<FString, int32> UpgradeLevels;
	UPROPERTY() int64 TotalClicks = 0;

	/** UTC time of the last save. Offline earnings are measured from here and claimed exactly once. */
	UPROPERTY() FDateTime LastSaveUtc;

	/** UTC time offline earnings were last granted (for diagnostics). */
	UPROPERTY() FDateTime LastOfflineClaimUtc;

	UPROPERTY() bool bReducedMotion = false;
};
