#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateTypes.h"

class UEvolutionGameSubsystem;
class FEvoEconomy;
class SEvoHelix;
class SScrollBox;
struct FEvoUpgradeDef;

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
	TSharedRef<SWidget> BuildStatTile(const FText& Label, TAttribute<FText> Value, TAttribute<FText> Tooltip);
	TSharedRef<SWidget> MakeUpgradeRow(const FString& UpgradeId);
	void RebuildUpgradeList();

	// ---- Event handlers ----
	void HandleStageChanged(int32 NewStageIndex);
	void HandleUpgradeBought(const FString& UpgradeId);
	void HandleProgressReset();
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
	bool bResetArmed = false;
	float ResetArmSeconds = 0.f;

	FText BannerTitle;
	FText BannerBody;
	float BannerSeconds = 0.f;
	float BannerDuration = 1.f;

	FDelegateHandle StageChangedHandle;
	FDelegateHandle UpgradeBoughtHandle;
	FDelegateHandle ResetHandle;

	// Brushes and styles must outlive the widgets that reference them.
	FSlateColorBrush WhiteBrush;
	FSlateRoundedBoxBrush PanelBrush;
	FSlateRoundedBoxBrush TileBrush;
	FSlateRoundedBoxBrush BannerBrush;
	FButtonStyle UpgradeButtonStyle;
	FButtonStyle PrimaryButtonStyle;
	FButtonStyle GhostButtonStyle;
	FProgressBarStyle ProgressStyle;
};
