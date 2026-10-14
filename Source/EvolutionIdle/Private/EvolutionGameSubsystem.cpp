#include "EvolutionGameSubsystem.h"
#include "EvolutionIdle.h"
#include "EvolutionSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"

const FString UEvolutionGameSubsystem::SaveSlotName = TEXT("EvolutionIdle_Save");

namespace
{
	constexpr int32 SaveUserIndex = 0;

	/** Longest single frame we will credit, so a debugger pause or hitch cannot produce absurd jumps. */
	constexpr double MaxTickSeconds = 60.0;
}

void UEvolutionGameSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LoadGameData();
	Economy.Init(&Data);

	if (bDataValid)
	{
		LoadProgress();
	}

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UEvolutionGameSubsystem::TickEconomy), 0.0f);
}

void UEvolutionGameSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	TickHandle.Reset();

	if (bDataValid)
	{
		SaveNow();
	}

	Super::Deinitialize();
}

void UEvolutionGameSubsystem::LoadGameData()
{
	const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data"), TEXT("GameData.json"));
	bDataValid = FEvoGameData::LoadFromFile(Path, Data, DataError);
	if (bDataValid)
	{
		UE_LOG(LogEvolution, Log, TEXT("Loaded %d stages and %d upgrades from %s"), Data.Stages.Num(), Data.Upgrades.Num(), *Path);
	}
	else
	{
		UE_LOG(LogEvolution, Error, TEXT("Game data failed to load: %s"), *DataError);
	}
}

void UEvolutionGameSubsystem::LoadProgress()
{
	if (!UGameplayStatics::DoesSaveGameExist(SaveSlotName, SaveUserIndex))
	{
		UE_LOG(LogEvolution, Log, TEXT("No save found; starting a new game."));
		SaveNow();
		return;
	}

	UEvolutionSaveGame* Save = Cast<UEvolutionSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, SaveUserIndex));
	if (!Save)
	{
		UE_LOG(LogEvolution, Warning, TEXT("Save slot exists but could not be read; starting a new game."));
		SaveNow();
		return;
	}

	FEvoState& State = Economy.GetState();
	State.CurrentEP = Save->CurrentEP;
	State.LifetimeEP = Save->LifetimeEP;
	State.StageIndex = Save->StageIndex;
	State.Levels = Save->UpgradeLevels;
	State.TotalClicks = Save->TotalClicks;
	Economy.SanitizeState();
	bReducedMotion = Save->bReducedMotion;

	// Offline earnings: measured from the last save, capped, granted once.
	// We save immediately afterwards so the same interval can never be claimed again.
	const FDateTime Now = FDateTime::UtcNow();
	const double Elapsed = (Now - Save->LastSaveUtc).GetTotalSeconds();
	double Counted = 0.0;
	const double Gain = Economy.ComputeOfflineGain(Elapsed, &Counted);
	if (Gain > 0.0)
	{
		Economy.Earn(Gain);
		PendingOfflineGain = Gain;
		PendingOfflineSeconds = Counted;
		UE_LOG(LogEvolution, Log, TEXT("Offline for %.0fs (counted %.0fs): +%.0f EP"), Elapsed, Counted, Gain);
	}

	SaveNow();
}

void UEvolutionGameSubsystem::SaveNow()
{
	if (!bDataValid)
	{
		return;
	}

	UEvolutionSaveGame* Save = Cast<UEvolutionSaveGame>(UGameplayStatics::CreateSaveGameObject(UEvolutionSaveGame::StaticClass()));
	if (!Save)
	{
		return;
	}

	const FEvoState& State = Economy.GetState();
	const FDateTime Now = FDateTime::UtcNow();
	Save->SaveVersion = UEvolutionSaveGame::CurrentVersion;
	Save->CurrentEP = State.CurrentEP;
	Save->LifetimeEP = State.LifetimeEP;
	Save->StageIndex = State.StageIndex;
	Save->UpgradeLevels = State.Levels;
	Save->TotalClicks = State.TotalClicks;
	Save->LastSaveUtc = Now;
	Save->LastOfflineClaimUtc = Now;
	Save->bReducedMotion = bReducedMotion;

	if (!UGameplayStatics::SaveGameToSlot(Save, SaveSlotName, SaveUserIndex))
	{
		UE_LOG(LogEvolution, Warning, TEXT("Failed to write save slot %s"), *SaveSlotName);
	}
	AutosaveTimer = 0.0;
}

bool UEvolutionGameSubsystem::TickEconomy(float DeltaTime)
{
	if (!bDataValid)
	{
		return true;
	}

	const double Seconds = FMath::Clamp(static_cast<double>(DeltaTime), 0.0, MaxTickSeconds);
	Economy.Advance(Seconds);

	AutosaveTimer += Seconds;
	if (AutosaveTimer >= Data.Economy.AutosaveSeconds)
	{
		SaveNow();
	}
	return true; // keep ticking
}

double UEvolutionGameSubsystem::Click()
{
	return bDataValid ? Economy.Click() : 0.0;
}

bool UEvolutionGameSubsystem::BuyUpgrade(const FString& UpgradeId)
{
	if (!bDataValid || !Economy.TryBuyUpgrade(UpgradeId))
	{
		return false;
	}
	OnUpgradeBought.Broadcast(UpgradeId);
	return true;
}

bool UEvolutionGameSubsystem::Evolve()
{
	if (!bDataValid || !Economy.TryEvolve())
	{
		return false;
	}
	SaveNow();
	OnStageChanged.Broadcast(Economy.GetState().StageIndex);
	return true;
}

void UEvolutionGameSubsystem::ResetProgress()
{
	if (!bDataValid)
	{
		return;
	}
	Economy.ResetState();
	PendingOfflineGain = 0.0;
	PendingOfflineSeconds = 0.0;
	SaveNow();
	OnProgressReset.Broadcast();
}

void UEvolutionGameSubsystem::SetReducedMotion(bool bEnabled)
{
	if (bReducedMotion != bEnabled)
	{
		bReducedMotion = bEnabled;
		SaveNow();
	}
}

void UEvolutionGameSubsystem::ClearOfflineReport()
{
	PendingOfflineGain = 0.0;
	PendingOfflineSeconds = 0.0;
}
