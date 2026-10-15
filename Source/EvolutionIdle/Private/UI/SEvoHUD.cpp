#include "UI/SEvoHUD.h"
#include "UI/SEvoHelix.h"
#include "UI/SEvoStageModel.h"
#include "UI/EvoFormat.h"
#include "EvolutionGameSubsystem.h"
#include "EvoEconomy.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SNullWidget.h"
#include "Algo/StableSort.h"

#define LOCTEXT_NAMESPACE "EvoHUD"

namespace
{
	const FLinearColor TextMain(0.80f, 0.90f, 1.0f, 1.f);
	const FLinearColor TextMuted(0.30f, 0.42f, 0.52f, 1.f);
	const FLinearColor TextWarn(1.0f, 0.45f, 0.30f, 1.f);

	// Escaped so the source stays ASCII.
	const TCHAR* Dot = TEXT(" · ");
	const TCHAR* Times = TEXT("×");

	FSlateFontInfo Font(const ANSICHAR* Weight, int32 Size)
	{
		return FCoreStyle::GetDefaultFontStyle(Weight, Size);
	}

	TSharedRef<STextBlock> Label(const FText& Text, int32 Size, const FLinearColor& Color, const ANSICHAR* Weight = "Regular")
	{
		return SNew(STextBlock).Text(Text).Font(Font(Weight, Size)).ColorAndOpacity(FSlateColor(Color));
	}

	FButtonStyle MakeButtonStyle(const FLinearColor& Fill, const FLinearColor& Outline, float Radius, const FMargin& Padding)
	{
		const FLinearColor Hover = Fill + FLinearColor(0.03f, 0.06f, 0.08f, 0.f);
		const FLinearColor Press = Fill * 0.7f;
		FLinearColor Disabled = Fill * 0.6f;
		Disabled.A = Fill.A * 0.7f;
		FLinearColor DisabledOutline = Outline;
		DisabledOutline.A *= 0.35f;

		FMargin Pressed = Padding;
		Pressed.Top += 1.f;
		Pressed.Bottom -= 1.f;

		return FButtonStyle()
			.SetNormal(FSlateRoundedBoxBrush(Fill, Radius, Outline, 1.f))
			.SetHovered(FSlateRoundedBoxBrush(Hover, Radius, Outline * 1.4f, 1.5f))
			.SetPressed(FSlateRoundedBoxBrush(Press, Radius, Outline, 1.f))
			.SetDisabled(FSlateRoundedBoxBrush(Disabled, Radius, DisabledOutline, 1.f))
			.SetNormalPadding(Padding)
			.SetPressedPadding(Pressed);
	}
}

SEvoHUD::SEvoHUD()
	: WhiteBrush(FLinearColor::White)
	, PanelBrush(FLinearColor(0.008f, 0.016f, 0.03f, 0.88f), 12.f, FLinearColor(0.10f, 0.35f, 0.50f, 0.35f), 1.f)
	, TileBrush(FLinearColor(0.012f, 0.025f, 0.045f, 0.92f), 8.f, FLinearColor(0.10f, 0.30f, 0.45f, 0.25f), 1.f)
	, BannerBrush(FLinearColor(0.006f, 0.014f, 0.03f, 0.96f), 14.f, FLinearColor(0.30f, 0.80f, 1.0f, 0.65f), 1.5f)
{
}

SEvoHUD::~SEvoHUD()
{
	if (UEvolutionGameSubsystem* Sub = Subsystem.Get())
	{
		Sub->OnStageChanged.Remove(StageChangedHandle);
		Sub->OnUpgradeBought.Remove(UpgradeBoughtHandle);
		Sub->OnProgressReset.Remove(ResetHandle);
	}
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

const FEvoEconomy* SEvoHUD::GetEconomy() const
{
	const UEvolutionGameSubsystem* Sub = Subsystem.Get();
	return (Sub && Sub->HasGameData()) ? &Sub->GetEconomy() : nullptr;
}

FLinearColor SEvoHUD::GetPrimary() const
{
	const FEvoEconomy* Econ = GetEconomy();
	const FEvoEraDef* Era = Econ ? Econ->GetCurrentEra() : nullptr;
	return Era ? Era->GetPrimary() : FLinearColor(0.02f, 0.75f, 1.0f);
}

FLinearColor SEvoHUD::GetSecondary() const
{
	const FEvoEconomy* Econ = GetEconomy();
	const FEvoEraDef* Era = Econ ? Econ->GetCurrentEra() : nullptr;
	return Era ? Era->GetSecondary() : FLinearColor(0.01f, 1.0f, 0.45f);
}

FLinearColor SEvoHUD::GetBackground() const
{
	const FEvoEconomy* Econ = GetEconomy();
	const FEvoEraDef* Era = Econ ? Econ->GetCurrentEra() : nullptr;
	return Era ? Era->GetBackground() : FLinearColor(0.002f, 0.004f, 0.008f);
}

FText SEvoHUD::GetUpgradeEffectText(const FEvoUpgradeDef& Def) const
{
	const FEvoEconomy* Econ = GetEconomy();
	if (!Econ)
	{
		return FText::GetEmpty();
	}
	switch (Def.GetEffect())
	{
	case EEvoEffect::Production:
		return FText::FromString(FString::Printf(TEXT("+%s EP/sec"), *EvoFormat::Number(Econ->GetMarginalEPPerSecond(Def))));
	case EEvoEffect::ClickPower:
		return FText::FromString(FString::Printf(TEXT("+%s EP per click"), *EvoFormat::Number(Econ->GetMarginalClickValue(Def))));
	case EEvoEffect::Multiplier:
		return FText::FromString(FString::Printf(TEXT("+%s all production"), *EvoFormat::Percent(Def.Amount)));
	default:
		return LOCTEXT("UnknownEffect", "Unknown effect");
	}
}

FText SEvoHUD::GetNextUnlockText() const
{
	const FEvoEconomy* Econ = GetEconomy();
	const FEvoStageDef* Current = Econ ? Econ->GetCurrentStage() : nullptr;
	if (!Current)
	{
		return FText::GetEmpty();
	}
	int32 NextUnlockStage = MAX_int32;
	int32 Count = 0;
	for (const FEvoUpgradeDef& Def : Econ->GetData().Upgrades)
	{
		if (Def.UnlockStage > Current->Id)
		{
			if (Def.UnlockStage < NextUnlockStage)
			{
				NextUnlockStage = Def.UnlockStage;
				Count = 1;
			}
			else if (Def.UnlockStage == NextUnlockStage)
			{
				++Count;
			}
		}
	}
	if (NextUnlockStage == MAX_int32)
	{
		return LOCTEXT("AllUnlocked", "All upgrades unlocked.");
	}
	FString StageName;
	for (const FEvoStageDef& Stage : Econ->GetData().Stages)
	{
		if (Stage.Id == NextUnlockStage)
		{
			StageName = Stage.Name;
			break;
		}
	}
	return FText::FromString(FString::Printf(TEXT("%d new upgrade%s unlock at stage %d: %s"),
		Count, Count == 1 ? TEXT("") : TEXT("s"), NextUnlockStage, *StageName));
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

void SEvoHUD::Construct(const FArguments& InArgs)
{
	Subsystem = InArgs._Subsystem;

	UpgradeButtonStyle = MakeButtonStyle(FLinearColor(0.012f, 0.03f, 0.055f, 0.95f), FLinearColor(0.15f, 0.5f, 0.7f, 0.45f), 8.f, FMargin(12.f, 10.f));
	PrimaryButtonStyle = MakeButtonStyle(FLinearColor(0.02f, 0.25f, 0.35f, 1.f), FLinearColor(0.3f, 0.9f, 1.f, 0.8f), 8.f, FMargin(22.f, 10.f));
	GhostButtonStyle = MakeButtonStyle(FLinearColor(0.01f, 0.02f, 0.035f, 0.6f), FLinearColor(0.2f, 0.4f, 0.55f, 0.4f), 6.f, FMargin(12.f, 6.f));

	ProgressStyle = FProgressBarStyle()
		.SetBackgroundImage(FSlateRoundedBoxBrush(FLinearColor(0.02f, 0.04f, 0.07f, 1.f), 5.f))
		.SetFillImage(FSlateRoundedBoxBrush(FLinearColor::White, 5.f))
		.SetMarqueeImage(FSlateRoundedBoxBrush(FLinearColor::White, 5.f));

	ChildSlot
	[
		SNew(SOverlay)

		// Era-tinted background.
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(&WhiteBrush)
			.ColorAndOpacity_Lambda([this]() { return FSlateColor(GetBackground()); })
		]

		// Main layout.
		+ SOverlay::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(16.f, 12.f, 16.f, 0.f)
			[
				BuildTopBar()
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			.Padding(16.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.Padding(0.f, 0.f, 16.f, 0.f)
				[
					BuildEvolutionArea()
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(440.f)
					[
						BuildUpgradePanel()
					]
				]
			]
		]

		// Banner (offline earnings, stage-up).
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.Padding(0.f, 96.f, 0.f, 0.f)
		[
			SNew(SBox)
			.MaxDesiredWidth(560.f)
			.Visibility(TAttribute<EVisibility>::CreateLambda([this]() { return BannerSeconds > 0.f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; }))
			[
				SNew(SBorder)
				.BorderImage(&BannerBrush)
				.Padding(FMargin(28.f, 16.f))
				.ColorAndOpacity_Lambda([this]()
				{
					const float Alpha = FMath::Clamp(FMath::Min(BannerSeconds / 0.4f, (BannerDuration - BannerSeconds) / 0.25f + 0.f), 0.f, 1.f);
					return FLinearColor(1.f, 1.f, 1.f, Alpha);
				})
				.BorderBackgroundColor_Lambda([this]()
				{
					const float Alpha = FMath::Clamp(BannerSeconds / 0.4f, 0.f, 1.f);
					return FSlateColor(FLinearColor(1.f, 1.f, 1.f, Alpha));
				})
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Font(Font("Bold", 22))
						.Justification(ETextJustify::Center)
						.Text_Lambda([this]() { return BannerTitle; })
						.ColorAndOpacity_Lambda([this]() { return FSlateColor(GetPrimary()); })
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					.Padding(0.f, 6.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Font(Font("Regular", 13))
						.AutoWrapText(true)
						.Justification(ETextJustify::Center)
						.ColorAndOpacity(FSlateColor(TextMain))
						.Text_Lambda([this]() { return BannerBody; })
					]
				]
			]
		]

		// Settings overlay.
		+ SOverlay::Slot()
		[
			BuildSettingsOverlay()
		]

		// Data error message.
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.BorderImage(&BannerBrush)
			.Padding(24.f)
			.Visibility(TAttribute<EVisibility>::CreateLambda([this]()
			{
				const UEvolutionGameSubsystem* Sub = Subsystem.Get();
				return (Sub && !Sub->HasGameData()) ? EVisibility::Visible : EVisibility::Collapsed;
			}))
			[
				SNew(STextBlock)
				.Font(Font("Regular", 14))
				.ColorAndOpacity(FSlateColor(TextWarn))
				.AutoWrapText(true)
				.Text_Lambda([this]()
				{
					const UEvolutionGameSubsystem* Sub = Subsystem.Get();
					return FText::FromString(FString::Printf(TEXT("Game data failed to load.\n%s"), Sub ? *Sub->GetDataError() : TEXT("")));
				})
			]
		]
	];

	if (UEvolutionGameSubsystem* Sub = Subsystem.Get())
	{
		StageChangedHandle = Sub->OnStageChanged.AddSP(this, &SEvoHUD::HandleStageChanged);
		UpgradeBoughtHandle = Sub->OnUpgradeBought.AddSP(this, &SEvoHUD::HandleUpgradeBought);
		ResetHandle = Sub->OnProgressReset.AddSP(this, &SEvoHUD::HandleProgressReset);

		if (Sub->GetPendingOfflineGain() > 0.0)
		{
			ShowBanner(
				LOCTEXT("WelcomeBack", "Welcome back"),
				FText::FromString(FString::Printf(TEXT("While you were away for %s, evolution continued: +%s EP"),
					*EvoFormat::Duration(Sub->GetPendingOfflineSeconds()), *EvoFormat::Number(Sub->GetPendingOfflineGain()))),
				5.f);
			Sub->ClearOfflineReport();
		}
	}

	RebuildUpgradeList();
	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SEvoHUD::TickHud));
}

TSharedRef<SWidget> SEvoHUD::BuildTopBar()
{
	return SNew(SBorder)
		.BorderImage(&PanelBrush)
		.Padding(FMargin(18.f, 10.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Title", "EVOLUTION IDLE"))
				.Font(Font("Bold", 18))
				.ColorAndOpacity_Lambda([this]() { return FSlateColor(GetPrimary()); })
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(24.f, 0.f)
			[
				SNew(STextBlock)
				.Font(Font("Regular", 13))
				.ColorAndOpacity(FSlateColor(TextMuted))
				.Text_Lambda([this]()
				{
					const FEvoEconomy* Econ = GetEconomy();
					const FEvoStageDef* Stage = Econ ? Econ->GetCurrentStage() : nullptr;
					const FEvoEraDef* Era = Econ ? Econ->GetCurrentEra() : nullptr;
					if (!Stage)
					{
						return FText::GetEmpty();
					}
					return FText::FromString(FString::Printf(TEXT("Stage %d / %d%sEra %d: %s"),
						Stage->Id, Econ->GetData().Stages.Num(), Dot, Stage->Era, Era ? *Era->Name : TEXT("")));
				})
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(SSpacer)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(16.f, 0.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				[
					Label(LOCTEXT("EPLabel", "EVOLUTION POINTS"), 9, TextMuted, "Bold")
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				[
					SNew(STextBlock)
					.Font(Font("Bold", 26))
					.ColorAndOpacity(FSlateColor(TextMain))
					.Text_Lambda([this]()
					{
						const FEvoEconomy* Econ = GetEconomy();
						return FText::FromString(Econ ? EvoFormat::Number(Econ->GetState().CurrentEP) : TEXT("0"));
					})
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(16.f, 0.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				[
					Label(LOCTEXT("LifetimeLabel", "LIFETIME EP"), 9, TextMuted, "Bold")
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				[
					SNew(STextBlock)
					.Font(Font("Regular", 16))
					.ColorAndOpacity(FSlateColor(TextMuted))
					.Text_Lambda([this]()
					{
						const FEvoEconomy* Econ = GetEconomy();
						return FText::FromString(Econ ? EvoFormat::Number(Econ->GetState().LifetimeEP) : TEXT("0"));
					})
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.ButtonStyle(&GhostButtonStyle)
				.OnClicked_Lambda([this]()
				{
					bSettingsOpen = !bSettingsOpen;
					bResetArmed = false;
					return FReply::Handled();
				})
				[
					Label(LOCTEXT("Settings", "Settings"), 12, TextMain)
				]
			]
		];
}

TSharedRef<SWidget> SEvoHUD::BuildEvolutionArea()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Font(Font("Bold", 30))
			.ColorAndOpacity(FSlateColor(TextMain))
			.Text_Lambda([this]()
			{
				const FEvoEconomy* Econ = GetEconomy();
				const FEvoStageDef* Stage = Econ ? Econ->GetCurrentStage() : nullptr;
				return Stage ? FText::FromString(Stage->Name) : FText::GetEmpty();
			})
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Font(Font("Regular", 13))
			.ColorAndOpacity(FSlateColor(TextMuted))
			.AutoWrapText(true)
			.Text_Lambda([this]()
			{
				const FEvoEconomy* Econ = GetEconomy();
				const FEvoStageDef* Stage = Econ ? Econ->GetCurrentStage() : nullptr;
				return Stage ? FText::FromString(Stage->Description) : FText::GetEmpty();
			})
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		.Padding(0.f, 8.f)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SEvoStageModel)
				.Subsystem(Subsystem)
			]
			+ SOverlay::Slot()
			[
				SAssignNew(Helix, SEvoHelix)
				.Subsystem(Subsystem)
			]
			// Next-evolution scanner in the gap to the right of the helix.
			+ SOverlay::Slot()
			[
				SNew(SHorizontalBox)
				.Visibility(EVisibility::SelfHitTestInvisible)
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(SSpacer)
					.Visibility(EVisibility::HitTestInvisible)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.Visibility(EVisibility::HitTestInvisible)
					.WidthOverride_Lambda([this]() -> FOptionalSize
					{
						// Same footprint as the helix drawing (see SEvoHelix::OnPaint), plus breathing room.
						const float Width = Helix.IsValid() ? static_cast<float>(Helix->GetTickSpaceGeometry().GetLocalSize().X) : 800.f;
						return 2.f * (FMath::Min(Width * 0.2f, 100.f) * 1.08f + 22.f);
					})
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 4.f, 0.f)
				[
					SNew(SBox)
					.MinDesiredWidth(150.f)
					.MaxDesiredWidth(300.f)
					[
						BuildEvolvePanel()
					]
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.f, 0.f, 0.f, 10.f)
		[
			SNew(STextBlock)
			.Font(Font("Regular", 11))
			.ColorAndOpacity(FSlateColor(TextMuted))
			.Text_Lambda([this]()
			{
				const FEvoEconomy* Econ = GetEconomy();
				const bool bSphere = Econ && Econ->IsCampaignComplete();
				return bSphere
					? LOCTEXT("ClickSphere", "Click the sphere of energy to generate EP")
					: LOCTEXT("ClickHelix", "Click the helix to generate Evolution Points");
			})
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			BuildProductionSummary()
		];
}

TSharedRef<SWidget> SEvoHUD::BuildStatTile(const FText& Title, TAttribute<FText> Value, TAttribute<FText> Tooltip)
{
	return SNew(SBorder)
		.BorderImage(&TileBrush)
		.Padding(FMargin(14.f, 8.f))
		.ToolTipText(Tooltip)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				Label(Title, 9, TextMuted, "Bold")
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(Font("Bold", 18))
				.Text(Value)
				.ColorAndOpacity_Lambda([this]() { return FSlateColor(GetPrimary()); })
			]
		];
}

TSharedRef<SWidget> SEvoHUD::BuildProductionSummary()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.Padding(0.f, 0.f, 8.f, 0.f)
		[
			BuildStatTile(LOCTEXT("PerClick", "EP PER CLICK"),
				TAttribute<FText>::CreateLambda([this]()
				{
					const FEvoEconomy* Econ = GetEconomy();
					return FText::FromString(Econ ? EvoFormat::Number(Econ->GetClickValue()) : TEXT("0"));
				}),
				LOCTEXT("PerClickTip", "EP earned for each click of the helix, after multipliers."))
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.Padding(0.f, 0.f, 8.f, 0.f)
		[
			BuildStatTile(LOCTEXT("PerSec", "EP PER SECOND"),
				TAttribute<FText>::CreateLambda([this]()
				{
					const FEvoEconomy* Econ = GetEconomy();
					return FText::FromString(Econ ? EvoFormat::Number(Econ->GetEPPerSecond()) : TEXT("0"));
				}),
				TAttribute<FText>::CreateLambda([this]()
				{
					const FEvoEconomy* Econ = GetEconomy();
					return FText::FromString(FString::Printf(TEXT("Passive production from auto-clickers.\nBase %s EP/sec before multipliers."),
						Econ ? *EvoFormat::Number(Econ->GetBaseEPPerSecond()) : TEXT("0")));
				}))
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		[
			BuildStatTile(LOCTEXT("Multiplier", "MULTIPLIER"),
				TAttribute<FText>::CreateLambda([this]()
				{
					const FEvoEconomy* Econ = GetEconomy();
					return FText::FromString(FString::Printf(TEXT("%s%.2f"), Times, Econ ? Econ->GetGlobalMultiplier() : 1.0));
				}),
				TAttribute<FText>::CreateLambda([this]()
				{
					const FEvoEconomy* Econ = GetEconomy();
					if (!Econ)
					{
						return FText::GetEmpty();
					}
					return FText::FromString(FString::Printf(TEXT("Applies to clicks and passive production.\nUpgrades %s%.2f%sEra bonuses %s%.2f"),
						Times, Econ->GetUpgradeMultiplier(), Dot, Times, Econ->GetStageBonusMultiplier()));
				}))
		];
}

TSharedRef<SWidget> SEvoHUD::BuildEvolvePanel()
{
	// Next-evolution panel that sits in the gap to the right of the helix:
	// a holographic scanner of the next form, progress toward its price, and the EVOLVE button.
	auto NextStage = [this]() -> const FEvoStageDef*
	{
		const FEvoEconomy* Econ = GetEconomy();
		return Econ ? Econ->GetNextStage() : nullptr;
	};
	auto Progress = [this]() -> float
	{
		const FEvoEconomy* Econ = GetEconomy();
		const FEvoStageDef* Next = Econ ? Econ->GetNextStage() : nullptr;
		if (!Next)
		{
			return 1.f;
		}
		return Next->Cost <= 0.0 ? 1.f : static_cast<float>(FMath::Clamp(Econ->GetState().CurrentEP / Next->Cost, 0.0, 1.0));
	};

	return SNew(SBorder)
		.BorderImage(&PanelBrush)
		.Padding(FMargin(14.f, 12.f))
		[
			SNew(SVerticalBox)
			// Header
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(Font("Bold", 10))
					.ColorAndOpacity(FSlateColor(TextMuted))
					.Text_Lambda([NextStage]()
					{
						return NextStage() ? LOCTEXT("ScanHeader", "SCANNING NEXT FORM") : LOCTEXT("ScanDone", "FINAL FORM");
					})
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(Font("Bold", 10))
					.ColorAndOpacity_Lambda([this]() { return FSlateColor(GetPrimary()); })
					.Text_Lambda([Progress]()
					{
						return FText::FromString(FString::Printf(TEXT("%d%%"), FMath::FloorToInt(Progress() * 100.f)));
					})
				]
			]
			// Scanner
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 6.f)
			[
				SNew(SBox)
				.HeightOverride_Lambda([this]() -> FOptionalSize
				{
					const float AreaHeight = Helix.IsValid() ? static_cast<float>(Helix->GetTickSpaceGeometry().GetLocalSize().Y) : 400.f;
					return FMath::Clamp(AreaHeight - 230.f, 90.f, 260.f);
				})
				[
					SNew(SEvoStageModel)
					.Subsystem(Subsystem)
					.ScannerMode(true)
				]
			]
			// Next stage name
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(Font("Bold", 15))
				.ColorAndOpacity(FSlateColor(TextMain))
				.AutoWrapText(true)
				.Text_Lambda([this, NextStage]()
				{
					const FEvoStageDef* Next = NextStage();
					if (!Next)
					{
						const FEvoEconomy* Econ = GetEconomy();
						const FEvoStageDef* Current = Econ ? Econ->GetCurrentStage() : nullptr;
						return Current ? FText::FromString(Current->Name) : FText::GetEmpty();
					}
					return FText::FromString(Next->Name);
				})
			]
			// Progress bar
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 6.f, 0.f, 4.f)
			[
				SNew(SBox)
				.HeightOverride(10.f)
				[
					SNew(SProgressBar)
					.Style(&ProgressStyle)
					.Percent_Lambda([Progress]() -> TOptional<float> { return Progress(); })
					.FillColorAndOpacity_Lambda([this]() { return FSlateColor(GetPrimary()); })
				]
			]
			// Have / need
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(Font("Bold", 12))
				.ColorAndOpacity_Lambda([this]() { return FSlateColor(GetPrimary()); })
				.Text_Lambda([this, NextStage]()
				{
					const FEvoEconomy* Econ = GetEconomy();
					const FEvoStageDef* Next = NextStage();
					if (!Econ || !Next)
					{
						return FText::GetEmpty();
					}
					return FText::FromString(FString::Printf(TEXT("%s / %s EP"),
						*EvoFormat::Number(FMath::Min(Econ->GetState().CurrentEP, Next->Cost)), *EvoFormat::Cost(Next->Cost)));
				})
			]
			// Status
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 2.f, 0.f, 8.f)
			[
				SNew(STextBlock)
				.Font(Font("Regular", 11))
				.AutoWrapText(true)
				.ColorAndOpacity_Lambda([this]()
				{
					const FEvoEconomy* Econ = GetEconomy();
					return FSlateColor((Econ && Econ->CanEvolve()) ? GetSecondary() : TextMuted);
				})
				.Text_Lambda([this]()
				{
					const FEvoEconomy* Econ = GetEconomy();
					if (!Econ)
					{
						return FText::GetEmpty();
					}
					const FEvoStageDef* Next = Econ->GetNextStage();
					if (!Next)
					{
						return LOCTEXT("Complete", "The campaign is complete. You have become a sphere of pure energy.");
					}
					if (Econ->CanEvolve())
					{
						return LOCTEXT("Ready", "Scan complete. Evolving spends the full price from your current EP.");
					}
					const double Shortfall = Next->Cost - Econ->GetState().CurrentEP;
					const double Rate = Econ->GetEPPerSecond();
					if (Rate > 0.0)
					{
						return FText::FromString(FString::Printf(TEXT("Need %s more%sabout %s"),
							*EvoFormat::Cost(Shortfall), Dot, *EvoFormat::Duration(Shortfall / Rate)));
					}
					return FText::FromString(FString::Printf(TEXT("Need %s more EP"), *EvoFormat::Cost(Shortfall)));
				})
			]
			// Evolve button
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(&PrimaryButtonStyle)
				.HAlign(HAlign_Center)
				.IsEnabled(TAttribute<bool>::CreateLambda([this]()
				{
					const FEvoEconomy* Econ = GetEconomy();
					return Econ && Econ->CanEvolve();
				}))
				.OnClicked_Lambda([this]()
				{
					if (UEvolutionGameSubsystem* Sub = Subsystem.Get())
					{
						Sub->Evolve();
					}
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Font(Font("Bold", 14))
					.ColorAndOpacity(FSlateColor(TextMain))
					.Text_Lambda([this]()
					{
						const FEvoEconomy* Econ = GetEconomy();
						return (Econ && Econ->IsCampaignComplete()) ? LOCTEXT("Done", "COMPLETE") : LOCTEXT("Evolve", "EVOLVE");
					})
				]
			]
		];
}

TSharedRef<SWidget> SEvoHUD::BuildUpgradePanel()
{
	return SNew(SBorder)
		.BorderImage(&PanelBrush)
		.Padding(FMargin(14.f, 14.f, 8.f, 14.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.f, 0.f, 6.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Upgrades", "UPGRADES"))
				.Font(Font("Bold", 16))
				.ColorAndOpacity_Lambda([this]() { return FSlateColor(GetPrimary()); })
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.f, 2.f, 6.f, 10.f)
			[
				Label(LOCTEXT("UpgradesHint", "Every purchase spends current EP and delays your next evolution."), 11, TextMuted)
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SAssignNew(UpgradeList, SScrollBox)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.f, 10.f, 6.f, 0.f)
			[
				SNew(STextBlock)
				.Font(Font("Italic", 11))
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor(TextMuted))
				.Text_Lambda([this]() { return GetNextUnlockText(); })
			]
		];
}

TSharedRef<SWidget> SEvoHUD::MakeUpgradeRow(const FString& UpgradeId)
{
	const FEvoEconomy* Econ = GetEconomy();
	const FEvoUpgradeDef* Def = Econ ? Econ->FindUpgrade(UpgradeId) : nullptr;
	if (!Def)
	{
		return SNullWidget::NullWidget;
	}

	auto FindDef = [this, UpgradeId]() -> const FEvoUpgradeDef*
	{
		const FEvoEconomy* E = GetEconomy();
		return E ? E->FindUpgrade(UpgradeId) : nullptr;
	};

	return SNew(SButton)
		.ButtonStyle(&UpgradeButtonStyle)
		.IsEnabled(TAttribute<bool>::CreateLambda([this, FindDef]()
		{
			const FEvoEconomy* E = GetEconomy();
			const FEvoUpgradeDef* D = FindDef();
			return E && D && E->CanBuyUpgrade(*D);
		}))
		.OnClicked_Lambda([this, UpgradeId]()
		{
			if (UEvolutionGameSubsystem* Sub = Subsystem.Get())
			{
				Sub->BuyUpgrade(UpgradeId);
			}
			return FReply::Handled();
		})
		[
			SNew(SVerticalBox)
			// Name + owned count
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					Label(FText::FromString(Def->Name), 14, TextMain, "Bold")
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(Font("Regular", 12))
					.ColorAndOpacity(FSlateColor(TextMuted))
					.Text_Lambda([this, FindDef]()
					{
						const FEvoEconomy* E = GetEconomy();
						const FEvoUpgradeDef* D = FindDef();
						if (!E || !D)
						{
							return FText::GetEmpty();
						}
						const int32 Level = E->GetLevel(D->Id);
						return FText::FromString(D->GetEffect() == EEvoEffect::Production
							? FString::Printf(TEXT("Owned %d"), Level)
							: FString::Printf(TEXT("Lv %d"), Level));
					})
				]
			]
			// Category + description
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 2.f, 0.f, 6.f)
			[
				SNew(STextBlock)
				.Font(Font("Regular", 11))
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor(TextMuted))
				.Text(FText::FromString(FString::Printf(TEXT("%s%s%s"), *Def->Category.ToUpper(), Dot, *Def->Description)))
			]
			// Benefit + cost
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(STextBlock)
					.Font(Font("Bold", 12))
					.ColorAndOpacity_Lambda([this]() { return FSlateColor(GetSecondary()); })
					.Text_Lambda([this, FindDef]()
					{
						const FEvoUpgradeDef* D = FindDef();
						return D ? GetUpgradeEffectText(*D) : FText::GetEmpty();
					})
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(Font("Bold", 12))
					.ColorAndOpacity_Lambda([this, FindDef]()
					{
						const FEvoEconomy* E = GetEconomy();
						const FEvoUpgradeDef* D = FindDef();
						const bool bAffordable = E && D && E->GetUpgradeCost(*D) <= E->GetState().CurrentEP;
						return FSlateColor(bAffordable ? TextMain : TextWarn);
					})
					.Text_Lambda([this, FindDef]()
					{
						const FEvoEconomy* E = GetEconomy();
						const FEvoUpgradeDef* D = FindDef();
						if (!E || !D)
						{
							return FText::GetEmpty();
						}
						if (E->IsMaxed(*D))
						{
							return LOCTEXT("Max", "MAX");
						}
						const double Cost = E->GetUpgradeCost(*D);
						const double Have = E->GetState().CurrentEP;
						if (Cost <= Have)
						{
							return FText::FromString(EvoFormat::Cost(Cost) + TEXT(" EP"));
						}
						return FText::FromString(FString::Printf(TEXT("%s EP%sneed %s"), *EvoFormat::Cost(Cost), Dot, *EvoFormat::Cost(Cost - Have)));
					})
				]
			]
			// Payback
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 2.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Font(Font("Regular", 10))
				.ColorAndOpacity(FSlateColor(TextMuted))
				.Text_Lambda([this, FindDef]()
				{
					const FEvoEconomy* E = GetEconomy();
					const FEvoUpgradeDef* D = FindDef();
					if (!E || !D || E->IsMaxed(*D))
					{
						return FText::GetEmpty();
					}
					const double Payback = E->GetPaybackSeconds(*D);
					if (Payback > 0.0)
					{
						return FText::FromString(FString::Printf(TEXT("Pays for itself in %s of production"), *EvoFormat::Duration(Payback)));
					}
					const double ClickGain = E->GetMarginalClickValue(*D);
					if (ClickGain > 0.0)
					{
						return FText::FromString(FString::Printf(TEXT("Pays for itself in about %s clicks"),
							*EvoFormat::Number(FMath::CeilToDouble(E->GetUpgradeCost(*D) / ClickGain))));
					}
					return FText::GetEmpty();
				})
			]
		];
}

TSharedRef<SWidget> SEvoHUD::BuildSettingsOverlay()
{
	return SNew(SBorder)
		.BorderImage(&WhiteBrush)
		.BorderBackgroundColor(FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.65f)))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Visibility(TAttribute<EVisibility>::CreateLambda([this]() { return bSettingsOpen ? EVisibility::Visible : EVisibility::Collapsed; }))
		[
			SNew(SBox)
			.WidthOverride(420.f)
			[
				SNew(SBorder)
				.BorderImage(&BannerBrush)
				.Padding(24.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 16.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SettingsTitle", "SETTINGS"))
						.Font(Font("Bold", 18))
						.ColorAndOpacity_Lambda([this]() { return FSlateColor(GetPrimary()); })
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 16.f)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([this]()
						{
							const UEvolutionGameSubsystem* Sub = Subsystem.Get();
							return (Sub && Sub->GetReducedMotion()) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						})
						.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
						{
							if (UEvolutionGameSubsystem* Sub = Subsystem.Get())
							{
								Sub->SetReducedMotion(NewState == ECheckBoxState::Checked);
							}
						})
						[
							SNew(SBox)
							.Padding(FMargin(8.f, 0.f, 0.f, 0.f))
							[
								Label(LOCTEXT("ReducedMotion", "Reduce motion (slower animation, no particles)"), 13, TextMain)
							]
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 16.f)
					[
						SNew(STextBlock)
						.Font(Font("Regular", 12))
						.ColorAndOpacity(FSlateColor(TextMuted))
						.Text_Lambda([this]()
						{
							const FEvoEconomy* Econ = GetEconomy();
							if (!Econ)
							{
								return FText::GetEmpty();
							}
							return FText::FromString(FString::Printf(TEXT("Total clicks: %lld\nLifetime EP: %s\nProgress saves automatically every few seconds."),
								Econ->GetState().TotalClicks, *EvoFormat::Number(Econ->GetState().LifetimeEP)));
						})
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SButton)
							.ButtonStyle(&GhostButtonStyle)
							.OnClicked_Lambda([this]()
							{
								if (bResetArmed)
								{
									bResetArmed = false;
									bSettingsOpen = false;
									if (UEvolutionGameSubsystem* Sub = Subsystem.Get())
									{
										Sub->ResetProgress();
									}
								}
								else
								{
									bResetArmed = true;
									ResetArmSeconds = 3.f;
								}
								return FReply::Handled();
							})
							[
								SNew(STextBlock)
								.Font(Font("Regular", 12))
								.ColorAndOpacity(FSlateColor(TextWarn))
								.Text_Lambda([this]()
								{
									return bResetArmed ? LOCTEXT("ConfirmReset", "Click again to erase all progress") : LOCTEXT("Reset", "Reset progress");
								})
							]
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						[
							SNew(SSpacer)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SButton)
							.ButtonStyle(&PrimaryButtonStyle)
							.OnClicked_Lambda([this]()
							{
								bSettingsOpen = false;
								bResetArmed = false;
								return FReply::Handled();
							})
							[
								Label(LOCTEXT("Close", "Close"), 13, TextMain, "Bold")
							]
						]
					]
				]
			]
		];
}

// ---------------------------------------------------------------------------
// Upgrade list
// ---------------------------------------------------------------------------

void SEvoHUD::RebuildUpgradeList()
{
	if (!UpgradeList.IsValid())
	{
		return;
	}
	UpgradeList->ClearChildren();

	const FEvoEconomy* Econ = GetEconomy();
	if (!Econ)
	{
		BuiltForStageIndex = INDEX_NONE;
		return;
	}

	// Newest tier first so freshly unlocked upgrades are visible without scrolling.
	TArray<const FEvoUpgradeDef*> Visible;
	for (const FEvoUpgradeDef& Def : Econ->GetData().Upgrades)
	{
		if (Econ->IsUnlocked(Def))
		{
			Visible.Add(&Def);
		}
	}
	Algo::StableSortBy(Visible, [](const FEvoUpgradeDef* Def) { return -Def->Era; });

	for (const FEvoUpgradeDef* Def : Visible)
	{
		UpgradeList->AddSlot()
		.Padding(FMargin(0.f, 0.f, 6.f, 8.f))
		[
			MakeUpgradeRow(Def->Id)
		];
	}
	BuiltForStageIndex = Econ->GetState().StageIndex;
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void SEvoHUD::ShowBanner(const FText& Title, const FText& Body, float Seconds)
{
	BannerTitle = Title;
	BannerBody = Body;
	BannerDuration = FMath::Max(0.5f, Seconds);
	BannerSeconds = BannerDuration;
}

void SEvoHUD::HandleStageChanged(int32 NewStageIndex)
{
	RebuildUpgradeList();
	if (Helix.IsValid())
	{
		Helix->PlayEvolveFlash();
	}

	const FEvoEconomy* Econ = GetEconomy();
	const FEvoStageDef* Stage = Econ ? Econ->GetCurrentStage() : nullptr;
	if (Stage)
	{
		const bool bFinal = Econ->IsCampaignComplete();
		ShowBanner(
			FText::FromString(bFinal
				? FString::Printf(TEXT("FINAL EVOLUTION: %s"), *Stage->Name.ToUpper())
				: FString::Printf(TEXT("EVOLVED: %s"), *Stage->Name.ToUpper())),
			FText::FromString(Stage->Description),
			(Subsystem.IsValid() && Subsystem->GetReducedMotion()) ? 2.f : 3.5f);
	}
}

void SEvoHUD::HandleUpgradeBought(const FString& UpgradeId)
{
	if (Helix.IsValid())
	{
		Helix->PlayPurchasePulse();
	}
}

void SEvoHUD::HandleProgressReset()
{
	RebuildUpgradeList();
	ShowBanner(LOCTEXT("ResetTitle", "A NEW BEGINNING"), LOCTEXT("ResetBody", "Progress has been reset to a single primordial cell."), 3.f);
}

EActiveTimerReturnType SEvoHUD::TickHud(double InCurrentTime, float InDeltaTime)
{
	BannerSeconds = FMath::Max(0.f, BannerSeconds - InDeltaTime);

	if (bResetArmed)
	{
		ResetArmSeconds -= InDeltaTime;
		if (ResetArmSeconds <= 0.f)
		{
			bResetArmed = false;
		}
	}

	// Safety net: if the stage changed without an event (e.g. data reload), refresh the unlocked upgrade list.
	if (const FEvoEconomy* Econ = GetEconomy())
	{
		if (Econ->GetState().StageIndex != BuiltForStageIndex)
		{
			RebuildUpgradeList();
		}
	}
	return EActiveTimerReturnType::Continue;
}

#undef LOCTEXT_NAMESPACE
