#include "EvoGameData.h"
#include "EvolutionIdle.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"

namespace
{
	FLinearColor ParseHexColor(const FString& Hex, const FLinearColor& Fallback)
	{
		FString Clean = Hex.TrimStartAndEnd();
		Clean.RemoveFromStart(TEXT("#"));
		if (Clean.Len() != 6 && Clean.Len() != 8)
		{
			return Fallback;
		}
		return FLinearColor(FColor::FromHex(Clean));
	}
}

FLinearColor FEvoEraDef::GetPrimary() const
{
	return ParseHexColor(PrimaryColor, FLinearColor(0.02f, 0.75f, 1.0f));
}

FLinearColor FEvoEraDef::GetSecondary() const
{
	return ParseHexColor(SecondaryColor, FLinearColor(0.01f, 1.0f, 0.45f));
}

FLinearColor FEvoEraDef::GetBackground() const
{
	return ParseHexColor(BackgroundColor, FLinearColor(0.002f, 0.004f, 0.008f));
}

EEvoEffect FEvoUpgradeDef::GetEffect() const
{
	if (Effect.Equals(TEXT("production"), ESearchCase::IgnoreCase))
	{
		return EEvoEffect::Production;
	}
	if (Effect.Equals(TEXT("click"), ESearchCase::IgnoreCase))
	{
		return EEvoEffect::ClickPower;
	}
	if (Effect.Equals(TEXT("multiplier"), ESearchCase::IgnoreCase))
	{
		return EEvoEffect::Multiplier;
	}
	return EEvoEffect::Unknown;
}

const FEvoEraDef* FEvoGameData::FindEra(int32 EraId) const
{
	return Eras.FindByPredicate([EraId](const FEvoEraDef& Era) { return Era.Id == EraId; });
}

double FEvoGameData::GetEraCostMultiplier(int32 EraId) const
{
	const FEvoEraDef* Era = FindEra(EraId);
	return (Era && Era->CostMultiplier > 0.0) ? Era->CostMultiplier : 1.0;
}

bool FEvoGameData::Finalize(TArray<FString>& OutErrors)
{
	bool bUsable = true;

	Stages.Sort([](const FEvoStageDef& A, const FEvoStageDef& B) { return A.Id < B.Id; });

	if (Stages.Num() == 0)
	{
		OutErrors.Add(TEXT("No stages defined."));
		bUsable = false;
	}
	else if (Stages[0].Cost != 0.0)
	{
		OutErrors.Add(FString::Printf(TEXT("First stage '%s' should cost 0 EP; forcing 0."), *Stages[0].Name));
		Stages[0].Cost = 0.0;
	}

	for (int32 Index = 0; Index < Stages.Num(); ++Index)
	{
		FEvoStageDef& Stage = Stages[Index];
		if (Index > 0 && Stage.Id == Stages[Index - 1].Id)
		{
			OutErrors.Add(FString::Printf(TEXT("Duplicate stage id %d."), Stage.Id));
			bUsable = false;
		}
		if (!FMath::IsFinite(Stage.Cost) || Stage.Cost < 0.0)
		{
			OutErrors.Add(FString::Printf(TEXT("Stage %d has an invalid cost; using 0."), Stage.Id));
			Stage.Cost = 0.0;
		}
		if (!FindEra(Stage.Era))
		{
			OutErrors.Add(FString::Printf(TEXT("Stage %d references unknown era %d."), Stage.Id, Stage.Era));
		}
	}

	TSet<FString> SeenIds;
	for (FEvoUpgradeDef& Upgrade : Upgrades)
	{
		if (Upgrade.Id.IsEmpty())
		{
			OutErrors.Add(FString::Printf(TEXT("Upgrade '%s' has no id."), *Upgrade.Name));
			bUsable = false;
			continue;
		}
		bool bAlreadySeen = false;
		SeenIds.Add(Upgrade.Id, &bAlreadySeen);
		if (bAlreadySeen)
		{
			OutErrors.Add(FString::Printf(TEXT("Duplicate upgrade id '%s'."), *Upgrade.Id));
			bUsable = false;
		}
		if (Upgrade.GetEffect() == EEvoEffect::Unknown)
		{
			OutErrors.Add(FString::Printf(TEXT("Upgrade '%s' has unknown effect '%s'."), *Upgrade.Id, *Upgrade.Effect));
		}
		if (!FMath::IsFinite(Upgrade.BaseCost) || Upgrade.BaseCost <= 0.0)
		{
			OutErrors.Add(FString::Printf(TEXT("Upgrade '%s' must have a positive base cost; using 1."), *Upgrade.Id));
			Upgrade.BaseCost = 1.0;
		}
		if (Upgrade.CostScaling != 0.0 && Upgrade.CostScaling < 1.0)
		{
			OutErrors.Add(FString::Printf(TEXT("Upgrade '%s' cost scaling below 1; using default."), *Upgrade.Id));
			Upgrade.CostScaling = 0.0;
		}
	}

	if (Economy.DefaultCostScaling < 1.0)
	{
		OutErrors.Add(TEXT("Economy defaultCostScaling below 1; using 1.2."));
		Economy.DefaultCostScaling = 1.2;
	}
	Economy.OfflineCapHours = FMath::Max(0.0, Economy.OfflineCapHours);
	Economy.OfflineEfficiency = FMath::Clamp(Economy.OfflineEfficiency, 0.0, 1.0);
	Economy.AutosaveSeconds = FMath::Max(1.0, Economy.AutosaveSeconds);

	return bUsable;
}

bool FEvoGameData::LoadFromString(const FString& Json, FEvoGameData& OutData, FString& OutError)
{
	OutData = FEvoGameData();
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &OutData))
	{
		OutError = TEXT("GameData.json could not be parsed. Check it for JSON syntax errors.");
		return false;
	}

	TArray<FString> Problems;
	const bool bUsable = OutData.Finalize(Problems);
	for (const FString& Problem : Problems)
	{
		UE_LOG(LogEvolution, Warning, TEXT("Game data: %s"), *Problem);
	}
	if (!bUsable)
	{
		OutError = FString::Join(Problems, TEXT("\n"));
	}
	return bUsable;
}

bool FEvoGameData::LoadFromFile(const FString& Path, FEvoGameData& OutData, FString& OutError)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		OutError = FString::Printf(TEXT("Could not read game data at %s"), *Path);
		return false;
	}
	return LoadFromString(Json, OutData, OutError);
}
