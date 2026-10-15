#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateTypes.h"
#include "GenericPlatform/GenericWindowDefinition.h"
#include "UI/EvoAchievementIcons.h"

class UEvolutionGameSubsystem;
class FEvoEconomy;
class SEvoHelix;
class SScrollBox;
struct FEvoUpgradeDef;
struct FEvoAchievementDef;

/**
 * Full-screen game interface built in Slate:
 * top status bar, central evolution area with the clickable helix, production summary,
 * next-evolution purchase panel, right-hand upgrade panel, banners and a settings overlay.
 */
class SEvoHUD : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEvoHUD) {}
		SLATE_ARGUMENT(UEvolutionGameSubsystem*, Subsystem)
	SLATE_END_ARGS()

	SEvoHUD();
	virtual ~SEvoHUD() override;

	void Construct(const FArguments& InArgs);

private:
	// ---- Layout builders ----
	TSharedRef<SWidget> BuildTopBar();
	TSharedRef<SWidget> BuildEvolutionArea();
	TSharedRef<SWidget> BuildProductionSummary();
	TSharedRef<SWidget> BuildEvolvePanel();
	TSharedRef<SWidget> BuildUpgradePanel();
	TSharedRef<SWidget> BuildSettingsOverlay();
	TSharedRef<SWidget> BuildAchievementsOverlay();
	TSharedRef<SWidget> BuildAchievementToast();
	TSharedRef<SWidget> MakeAchievementRow(const FString& AchievementId);
	void RebuildAchievementList();
	TSharedRef<SWidget> BuildStatTile(const FText& Label, TAttribute<FText> Value, TAttribute<FText> Tooltip);
	TSharedRef<SWidget> MakeUpgradeRow(const FString& UpgradeId);
	void RebuildUpgradeList();

	/** A "Title   <  value  >" row used by the display settings. */
	TSharedRef<SWidget> MakeCycler(const FText& Title, TAttribute<FText> Value, TFunction<void(int32)> OnStep, TAttribute<bool> Enabled);
	void RefreshDisplayChoices();
	void RefreshResolutionList();

	// ---- Event handlers ----
	void HandleStageChanged(int32 NewStageIndex);
	void HandleUpgradeBought(const FString& UpgradeId);
	void HandleProgressReset();
	void HandleAchievementUnlocked(const FString& AchievementId);
	EActiveTimerReturnType TickHud(double InCurrentTime, float InDeltaTime);
	void ShowBanner(const FText& Title, const FText& Body, float Seconds);

	// ---- Helpers ----
	const FEvoEconomy* GetEconomy() const;
	FLinearColor GetPrimary() const;
	FLinearColor GetSecondary() const;
	FLinearColor GetBackground() const;
	FText GetUpgradeEffectText(const FEvoUpgradeDef& Def) const;
	FText GetNextUnlockText() const;

	TWeakObjectPtr<UEvolutionGameSubsystem> Subsystem;
	TSharedPtr<SEvoHelix> Helix;
	TSharedPtr<SScrollBox> UpgradeList;
	int32 BuiltForStageIndex = INDEX_NONE;

	bool bSettingsOpen = false;
	bool bAchievementsOpen = false;
	TSharedPtr<SScrollBox> AchievementList;
	FEvoAchievementIcons AchievementIcons;

	// Achievement unlock toasts, shown one at a time.
	TArray<FString> ToastQueue;
	FString ToastId;
	float ToastSeconds = 0.f;

	// Display settings being edited (applied with the Apply button).
	EWindowMode::Type PendingMode = EWindowMode::Windowed;
	FIntPoint PendingResolution = FIntPoint(1280, 720);
	int32 PendingMonitor = 0;
	TArray<FIntPoint> ResolutionChoices;
	bool bResetArmed = false;
	float ResetArmSeconds = 0.f;

	FText BannerTitle;
	FText BannerBody;
	float BannerSeconds = 0.f;
	float BannerDuration = 1.f;

	FDelegateHandle StageChangedHandle;
	FDelegateHandle UpgradeBoughtHandle;
	FDelegateHandle ResetHandle;
	FDelegateHandle AchievementHandle;

	// Brushes and styles must outlive the widgets that reference them.
	FSlateColorBrush WhiteBrush;
	FSlateRoundedBoxBrush PanelBrush;
	FSlateRoundedBoxBrush TileBrush;
	FSlateRoundedBoxBrush BannerBrush;
	FSlateRoundedBoxBrush BadgeBrush;
	FButtonStyle UpgradeButtonStyle;
	FButtonStyle PrimaryButtonStyle;
	FButtonStyle GhostButtonStyle;
	FProgressBarStyle ProgressStyle;
};
