#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Brushes/SlateColorBrush.h"
#include "Math/RandomStream.h"

class UEvolutionGameSubsystem;

/**
 * The clickable centerpiece. Draws an animated DNA double helix that evolves with the eras:
 * organic helix (stages 1-12) -> circuit helix (13-16) -> helix around an energy core (17-19)
 * -> a luminous sphere of pure energy (20). Clicking anywhere on it earns EP.
 */
class SEvoHelix : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SEvoHelix) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UEvolutionGameSubsystem>, Subsystem)
	SLATE_END_ARGS()

	SEvoHelix();

	void Construct(const FArguments& InArgs);

	/** Bright expanding ring used when a new stage is purchased. */
	void PlayEvolveFlash();

	/** Small feedback pulse (e.g. after buying an upgrade). */
	void PlayPurchasePulse();

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

private:
	struct FFloatText
	{
		FVector2f Pos;
		float Age = 0.f;
		FString Text;
	};

	struct FParticle
	{
		FVector2f Pos;
		FVector2f Vel;
		float Age = 0.f;
		float Life = 1.f;
		float Radius = 3.f;
		bool bFromCenter = false; // spawned by the evolve flash; positioned relative to the widget centre
	};

	EActiveTimerReturnType Animate(double InCurrentTime, float InDeltaTime);
	FReply HandleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	bool IsReducedMotion() const;
	void GetPalette(FLinearColor& OutPrimary, FLinearColor& OutSecondary) const;
	int32 GetStageId() const;

	void DrawCircle(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2f& Center, float Radius, const FLinearColor& Color) const;
	void DrawSquare(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2f& Center, float HalfSize, const FLinearColor& Color) const;
	void DrawRing(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2f& Center, float RadiusX, float RadiusY, float Angle, const FLinearColor& Color, float Thickness) const;

	TWeakObjectPtr<UEvolutionGameSubsystem> Subsystem;

	float Time = 0.f;
	float Pulse = 0.f;
	float Flash = 0.f;
	TArray<FFloatText> Floaters;
	TArray<FParticle> Particles;
	FRandomStream Rng;

	/** Rounded-box brushes whose corner radius exactly equals half their draw size, giving true circles. */
	TArray<FSlateRoundedBoxBrush> CircleBrushes;
	TArray<float> CircleRadii;
	FSlateColorBrush SquareBrush;
};
