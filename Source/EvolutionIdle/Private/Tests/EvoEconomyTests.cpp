// Automation tests for the GDD section 15 acceptance criteria that do not need a running world.
// Run them in the editor: Tools > Session Frontend > Automation, filter "EvolutionIdle".

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "EvoEconomy.h"
#include "EvoGameData.h"
#include "EvoAchievements.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace EvoTest
{
	const TCHAR* Json = TEXT(R"JSON(
	{
		"economy": { "baseClickPower": 1, "defaultCostScaling": 1.2, "offlineCapHours": 8, "offlineEfficiency": 0.5, "autosaveSeconds": 15 },
		"eras": [
			{ "id": 1, "name": "One", "costMultiplier": 1 },
			{ "id": 2, "name": "Two", "costMultiplier": 5 }
		],
		"stages": [
			{ "id": 2, "era": 1, "cost": 250, "name": "Second" },
			{ "id": 1, "era": 1, "cost": 0, "name": "First" },
			{ "id": 3, "era": 2, "cost": 1000, "productionBonus": 0.1, "name": "Third" }
		],
		"upgrades": [
			{ "id": "unit", "era": 1, "unlockStage": 1, "effect": "production", "baseCost": 50, "amount": 1, "name": "Unit" },
			{ "id": "click", "era": 1, "unlockStage": 1, "effect": "click", "baseCost": 500, "amount": 1, "costScaling": 1.5, "name": "Click" },
			{ "id": "mult", "era": 1, "unlockStage": 1, "effect": "multiplier", "baseCost": 100, "amount": 0.25, "costScaling": 4, "maxLevel": 2, "name": "Mult" },
			{ "id": "late", "era": 2, "unlockStage": 3, "effect": "production", "baseCost": 10, "amount": 10, "name": "Late" }
		],
		"achievements": [
			{ "id": "ACH_STAGE_2", "type": "stage", "target": 2, "name": "Two" },
			{ "id": "ACH_ERA_1", "type": "eraComplete", "target": 1, "name": "Era one" },
			{ "id": "ACH_CLICKS_3", "type": "clicks", "target": 3, "name": "Clicks" },
			{ "id": "ACH_AUTO_2", "type": "autoClickers", "target": 2, "name": "Auto" },
			{ "id": "ACH_ALL", "type": "allUpgrades", "target": 1, "name": "All" },
			{ "id": "ACH_OFFLINE", "type": "offlineFull", "target": 1, "name": "Offline" }
		]
	}
	)JSON");

	bool Setup(FAutomationTestBase& Test, FEvoGameData& Data, FEvoEconomy& Econ)
	{
		FString Error;
		const bool bLoaded = FEvoGameData::LoadFromString(Json, Data, Error);
		Test.TestTrue(FString::Printf(TEXT("Test data loads (%s)"), *Error), bLoaded);
		Econ.Init(&Data);
		return bLoaded;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEvoDataLoadTest, "EvolutionIdle.Economy.DataLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FEvoDataLoadTest::RunTest(const FString& Parameters)
{
	FEvoGameData Data;
	FEvoEconomy Econ;
	if (!EvoTest::Setup(*this, Data, Econ)) { return false; }

	TestEqual(TEXT("Stages are sorted by id"), Data.Stages[0].Id, 1);
	TestEqual(TEXT("Stage count"), Data.Stages.Num(), 3);
	TestEqual(TEXT("Upgrade effect parsed"), static_cast<int32>(Data.Upgrades[1].GetEffect()), static_cast<int32>(EEvoEffect::ClickPower));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEvoClickTest, "EvolutionIdle.Economy.ClickAddsToCurrentAndLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FEvoClickTest::RunTest(const FString& Parameters)
{
	FEvoGameData Data;
	FEvoEconomy Econ;
	if (!EvoTest::Setup(*this, Data, Econ)) { return false; }

	const double Gain = Econ.Click();
	TestEqual(TEXT("Base click is 1 EP"), Gain, 1.0);
	TestEqual(TEXT("Current EP"), Econ.GetState().CurrentEP, 1.0);
	TestEqual(TEXT("Lifetime EP"), Econ.GetState().LifetimeEP, 1.0);
	TestEqual(TEXT("Click counted"), Econ.GetState().TotalClicks, static_cast<int64>(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEvoUpgradePurchaseTest, "EvolutionIdle.Economy.UpgradePurchase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FEvoUpgradePurchaseTest::RunTest(const FString& Parameters)
{
	FEvoGameData Data;
	FEvoEconomy Econ;
	if (!EvoTest::Setup(*this, Data, Econ)) { return false; }

	const FEvoUpgradeDef* Unit = Econ.FindUpgrade(TEXT("unit"));
	TestNotNull(TEXT("Unit exists"), Unit);
	if (!Unit) { return false; }

	// Unaffordable purchases are blocked and change nothing.
	Econ.Earn(49.0);
	TestFalse(TEXT("Cannot buy at 49 EP"), Econ.TryBuyUpgrade(TEXT("unit")));
	TestEqual(TEXT("Balance untouched"), Econ.GetState().CurrentEP, 49.0);
	TestEqual(TEXT("Level untouched"), Econ.GetLevel(TEXT("unit")), 0);

	// Purchase deducts exactly the displayed cost and applies once.
	Econ.Earn(1.0);
	const double Displayed = Econ.GetUpgradeCost(*Unit);
	double Paid = 0.0;
	TestTrue(TEXT("Can buy at 50 EP"), Econ.TryBuyUpgrade(TEXT("unit"), &Paid));
	TestEqual(TEXT("Paid displayed cost"), Paid, Displayed);
	TestEqual(TEXT("Balance is zero, never negative"), Econ.GetState().CurrentEP, 0.0);
	TestEqual(TEXT("Lifetime never decreases"), Econ.GetState().LifetimeEP, 50.0);
	TestEqual(TEXT("Level 1"), Econ.GetLevel(TEXT("unit")), 1);
	TestEqual(TEXT("EP/sec applied"), Econ.GetEPPerSecond(), 1.0);

	// Price scaling: floor(50 * 1.2^1 * 1) = 60, then floor(50 * 1.2^2) = 72.
	TestEqual(TEXT("Second cost"), Econ.GetUpgradeCost(*Unit), 60.0);
	Econ.Earn(60.0);
	Econ.TryBuyUpgrade(TEXT("unit"));
	TestEqual(TEXT("Third cost"), Econ.GetUpgradeCost(*Unit), 72.0);

	// Locked upgrades cannot be bought regardless of balance.
	Econ.Earn(1.0e9);
	TestFalse(TEXT("Locked upgrade blocked"), Econ.TryBuyUpgrade(TEXT("late")));

	// Max level respected.
	TestTrue(TEXT("Mult 1"), Econ.TryBuyUpgrade(TEXT("mult")));
	TestTrue(TEXT("Mult 2"), Econ.TryBuyUpgrade(TEXT("mult")));
	TestFalse(TEXT("Mult capped"), Econ.TryBuyUpgrade(TEXT("mult")));
	TestEqual(TEXT("Multiplier 1.5"), Econ.GetGlobalMultiplier(), 1.5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEvoEvolveTest, "EvolutionIdle.Economy.EvolutionPurchase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FEvoEvolveTest::RunTest(const FString& Parameters)
{
	FEvoGameData Data;
	FEvoEconomy Econ;
	if (!EvoTest::Setup(*this, Data, Econ)) { return false; }

	Econ.Earn(249.0);
	TestFalse(TEXT("Cannot evolve short of price"), Econ.TryEvolve());
	TestEqual(TEXT("Still stage 1"), Econ.GetState().StageIndex, 0);

	Econ.Earn(51.0);
	double Paid = 0.0;
	TestTrue(TEXT("Evolves with enough current EP"), Econ.TryEvolve(&Paid));
	TestEqual(TEXT("Paid stage price"), Paid, 250.0);
	TestEqual(TEXT("Remaining EP"), Econ.GetState().CurrentEP, 50.0);
	TestEqual(TEXT("Lifetime unchanged by spending"), Econ.GetState().LifetimeEP, 300.0);
	TestEqual(TEXT("Now stage 2"), Econ.GetCurrentStage()->Id, 2);

	Econ.Earn(950.0);
	TestTrue(TEXT("Evolve to stage 3"), Econ.TryEvolve());
	TestTrue(TEXT("Campaign complete"), Econ.IsCampaignComplete());
	TestFalse(TEXT("No evolution past the end"), Econ.TryEvolve());
	TestEqual(TEXT("Stage bonus applies"), Econ.GetStageBonusMultiplier(), 1.1);

	const FEvoUpgradeDef* Late = Econ.FindUpgrade(TEXT("late"));
	TestTrue(TEXT("Late upgrade unlocked"), Late && Econ.IsUnlocked(*Late));
	TestEqual(TEXT("Era cost multiplier applied"), Late ? Econ.GetUpgradeCost(*Late) : 0.0, 50.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEvoProductionTest, "EvolutionIdle.Economy.ProductionAndOffline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FEvoProductionTest::RunTest(const FString& Parameters)
{
	FEvoGameData Data;
	FEvoEconomy Econ;
	if (!EvoTest::Setup(*this, Data, Econ)) { return false; }

	Econ.GetState().Levels.Add(TEXT("unit"), 10);
	TestEqual(TEXT("10 EP/sec"), Econ.GetEPPerSecond(), 10.0);

	// Production over a measured interval matches the displayed rate.
	for (int32 Frame = 0; Frame < 600; ++Frame)
	{
		Econ.Advance(1.0 / 60.0);
	}
	TestTrue(TEXT("~100 EP after 10s"), FMath::IsNearlyEqual(Econ.GetState().CurrentEP, 100.0, 1e-6));

	// Offline: 50% efficiency, 8h cap, never negative.
	TestEqual(TEXT("1h offline"), Econ.ComputeOfflineGain(3600.0), 10.0 * 3600.0 * 0.5);
	double Counted = 0.0;
	TestEqual(TEXT("24h capped to 8h"), Econ.ComputeOfflineGain(24.0 * 3600.0, &Counted), 10.0 * 8.0 * 3600.0 * 0.5);
	TestEqual(TEXT("Counted seconds capped"), Counted, 8.0 * 3600.0);
	TestEqual(TEXT("Clock moved backwards"), Econ.ComputeOfflineGain(-500.0), 0.0);

	// Bad values are ignored.
	const double Before = Econ.GetState().CurrentEP;
	Econ.Earn(-5.0);
	Econ.Earn(std::numeric_limits<double>::quiet_NaN());
	TestEqual(TEXT("Negative/NaN earnings ignored"), Econ.GetState().CurrentEP, Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEvoSanitizeTest, "EvolutionIdle.Economy.SanitizeLoadedState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FEvoSanitizeTest::RunTest(const FString& Parameters)
{
	FEvoGameData Data;
	FEvoEconomy Econ;
	if (!EvoTest::Setup(*this, Data, Econ)) { return false; }

	FEvoState& State = Econ.GetState();
	State.CurrentEP = -10.0;
	State.LifetimeEP = -1.0;
	State.StageIndex = 99;
	State.Levels.Add(TEXT("does_not_exist"), 3);
	State.Levels.Add(TEXT("mult"), 50);
	Econ.SanitizeState();

	TestEqual(TEXT("EP clamped"), State.CurrentEP, 0.0);
	TestEqual(TEXT("Lifetime clamped"), State.LifetimeEP, 0.0);
	TestEqual(TEXT("Stage clamped"), State.StageIndex, 2);
	TestFalse(TEXT("Unknown upgrade dropped"), State.Levels.Contains(TEXT("does_not_exist")));
	TestEqual(TEXT("Level clamped to max"), Econ.GetLevel(TEXT("mult")), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEvoAchievementTest, "EvolutionIdle.Achievements.Conditions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FEvoAchievementTest::RunTest(const FString& Parameters)
{
	FEvoGameData Data;
	FEvoEconomy Econ;
	if (!EvoTest::Setup(*this, Data, Econ)) { return false; }

	TestEqual(TEXT("Achievements loaded"), Data.Achievements.Num(), 6);
	auto Done = [&Econ, &Data](int32 Index, double Offline = 0.0)
	{
		return EvoAchievements::Evaluate(Data.Achievements[Index], Econ, Offline).IsComplete();
	};

	// Fresh game: nothing unlocked.
	for (int32 Index = 0; Index < Data.Achievements.Num(); ++Index)
	{
		TestFalse(FString::Printf(TEXT("%s locked at start"), *Data.Achievements[Index].Id), Done(Index));
	}

	// Stage 2 is the last stage of era 1.
	TestEqual(TEXT("Last stage of era 1"), EvoAchievements::GetLastStageOfEra(Data, 1), 2);
	Econ.GetState().StageIndex = 1;
	TestTrue(TEXT("Stage achievement"), Done(0));
	TestTrue(TEXT("Era complete achievement"), Done(1));

	// Clicks.
	Econ.Click();
	Econ.Click();
	TestFalse(TEXT("2 of 3 clicks"), Done(2));
	Econ.Click();
	TestTrue(TEXT("3 clicks"), Done(2));

	// Auto-clickers count every production upgrade's levels.
	Econ.GetState().Levels.Add(TEXT("unit"), 1);
	TestFalse(TEXT("1 auto-clicker"), Done(3));
	Econ.GetState().Levels.Add(TEXT("late"), 1);
	TestEqual(TEXT("Auto-clicker count"), EvoAchievements::CountAutoClickers(Econ), 2);
	TestTrue(TEXT("2 auto-clickers"), Done(3));

	// Every upgrade at least once.
	TestFalse(TEXT("Not every upgrade owned"), Done(4));
	Econ.GetState().Levels.Add(TEXT("click"), 1);
	Econ.GetState().Levels.Add(TEXT("mult"), 1);
	TestTrue(TEXT("Every upgrade owned"), Done(4));

	// Offline: needs the full cap (8h in the test data).
	TestFalse(TEXT("7h offline is not enough"), Done(5, 7.0 * 3600.0));
	TestTrue(TEXT("Full 8h offline"), Done(5, 8.0 * 3600.0));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
