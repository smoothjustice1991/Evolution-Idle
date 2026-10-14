#pragma once

#include "CoreMinimal.h"
#include "EvoGameData.generated.h"

/**
 * Data definitions loaded from Content/Data/GameData.json.
 * JSON keys are camelCase versions of the property names (e.g. BaseCost -> "baseCost").
 */

USTRUCT()
struct EVOLUTIONIDLE_API FEvoEconomyConfig
{
	GENERATED_BODY()

	/** EP granted per click before upgrades and multipliers. */
	UPROPERTY() double BaseClickPower = 1.0;

	/** Repeated-purchase factor r in Cn = floor(C0 * r^n * S), used when an upgrade has no CostScaling. */
	UPROPERTY() double DefaultCostScaling = 1.2;

	/** Maximum offline duration that earns EP. */
	UPROPERTY() double OfflineCapHours = 8.0;

	/** Fraction of normal passive production granted while offline. */
	UPROPERTY() double OfflineEfficiency = 0.5;

	/** Seconds between automatic saves. */
	UPROPERTY() double AutosaveSeconds = 15.0;
};

USTRUCT()
struct EVOLUTIONIDLE_API FEvoEraDef
{
	GENERATED_BODY()

	UPROPERTY() int32 Id = 0;
	UPROPERTY() FString Name;

	/** Era cost multiplier S applied to upgrades that belong to this era. */
	UPROPERTY() double CostMultiplier = 1.0;

	/** Hex colours (RRGGBB) used to theme the UI and helix during this era. */
	UPROPERTY() FString PrimaryColor;
	UPROPERTY() FString SecondaryColor;
	UPROPERTY() FString BackgroundColor;

	FLinearColor GetPrimary() const;
	FLinearColor GetSecondary() const;
	FLinearColor GetBackground() const;
};

USTRUCT()
struct EVOLUTIONIDLE_API FEvoStageDef
{
	GENERATED_BODY()

	UPROPERTY() int32 Id = 0;
	UPROPERTY() int32 Era = 1;
	UPROPERTY() FString Name;
	UPROPERTY() FString Description;

	/** Current-EP price to evolve INTO this stage (deducted on purchase). */
	UPROPERTY() double Cost = 0.0;

	/** Permanent additive production bonus granted once this stage is reached (0.1 = +10%). */
	UPROPERTY() double ProductionBonus = 0.0;

	/** Art reference key. Placeholder until real art exists; the helix renderer uses the stage id instead. */
	UPROPERTY() FString Art;
};

enum class EEvoEffect : uint8
{
	Production,  // +Amount EP/sec per level (auto-clicker unit)
	ClickPower,  // +Amount EP per manual click per level
	Multiplier,  // +Amount (e.g. 0.25 = +25%) to the global multiplier per level
	Unknown
};

USTRUCT()
struct EVOLUTIONIDLE_API FEvoUpgradeDef
{
	GENERATED_BODY()

	UPROPERTY() FString Id;
	UPROPERTY() FString Name;
	UPROPERTY() FString Description;
	UPROPERTY() FString Category;

	/** "production", "click" or "multiplier". */
	UPROPERTY() FString Effect;

	UPROPERTY() double BaseCost = 0.0;

	/** Per-upgrade repeated-purchase factor; 0 means use the economy default. */
	UPROPERTY() double CostScaling = 0.0;

	UPROPERTY() double Amount = 0.0;

	/** Stage id at which this upgrade becomes available. */
	UPROPERTY() int32 UnlockStage = 1;

	/** Era whose cost multiplier applies to this upgrade. */
	UPROPERTY() int32 Era = 1;

	/** 0 = unlimited. */
	UPROPERTY() int32 MaxLevel = 0;

	EEvoEffect GetEffect() const;
};

USTRUCT()
struct EVOLUTIONIDLE_API FEvoGameData
{
	GENERATED_BODY()

	UPROPERTY() FEvoEconomyConfig Economy;
	UPROPERTY() TArray<FEvoEraDef> Eras;
	UPROPERTY() TArray<FEvoStageDef> Stages;
	UPROPERTY() TArray<FEvoUpgradeDef> Upgrades;

	const FEvoEraDef* FindEra(int32 EraId) const;
	double GetEraCostMultiplier(int32 EraId) const;

	/** Sorts stages by id and checks the data for mistakes. Returns false if the data is unusable. */
	bool Finalize(TArray<FString>& OutErrors);

	/** Loads and finalizes data from a JSON file. */
	static bool LoadFromFile(const FString& Path, FEvoGameData& OutData, FString& OutError);

	/** Parses and finalizes data from a JSON string. */
	static bool LoadFromString(const FString& Json, FEvoGameData& OutData, FString& OutError);
};
