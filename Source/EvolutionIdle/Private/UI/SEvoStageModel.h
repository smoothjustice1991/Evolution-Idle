#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/RenderingCommon.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "UI/EvoStageMeshes.h"

class UEvolutionGameSubsystem;
struct FSlateBrush;

/**
 * Background "hologram" of the current evolution stage: a low-poly 3D model that floats, slowly
 * turns and sways behind the helix. Models are generated in EvoStageMeshes.cpp, shaded here on the
 * CPU (flat lighting, rim light, depth fade) and drawn as one batch of Slate triangles.
 *
 * Upgrades show up on the model:
 *  - Auto-clickers: crystals orbiting the model, one ring per auto-clicker type (newest 4 types),
 *    with more crystals as you own more (1, 2-3, 4-7, 8-15, 16-31, 32+).
 *  - Click power: a crown of glowing shards above the model, one per level (up to 10), and a
 *    brighter glow on the model's accent parts.
 *  - Multipliers: extra glowing halo rings on the pedestal, one per level (up to 5).
 *  - Every purchase: the model flashes and pulses, sparks burst out and "+1 <name>" floats up.
 * When the stage changes, the old model shrinks away while the new one grows in.
 */
class SEvoStageModel : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SEvoStageModel)
		: _ScannerMode(false)
	{}
		SLATE_ARGUMENT(TWeakObjectPtr<UEvolutionGameSubsystem>, Subsystem)
		/** Scanner mode: shows the NEXT stage as a hologram that is revealed bottom-up as EP approaches its price. */
		SLATE_ARGUMENT(bool, ScannerMode)
	SLATE_END_ARGS()

	SEvoStageModel();
	virtual ~SEvoStageModel() override;

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

private:
	struct FPalette
	{
		FLinearColor Primary;
		FLinearColor Secondary;
		FLinearColor Background;
	};

	struct FSortedTri
	{
		float Depth = 0.f;
		int32 MeshIndex = 0;
		int32 Index = 0;
		FVector3f Normal = FVector3f::ZeroVector;
	};

	/** One ring of orbiting crystals for an owned auto-clicker type. */
	struct FOrbitRing
	{
		FString UpgradeId;
		int32 Crystals = 0;
		int32 RingIndex = 0;
	};

	/** Summary of owned upgrades, recomputed each frame from the economy. */
	struct FUpgradeVisuals
	{
		TArray<FOrbitRing> Rings;
		int32 ClickLevels = 0;
		int32 MultiplierLevels = 0;
	};

	struct FSpark
	{
		FVector2f Pos;     // in units of the model radius, relative to the model centre
		FVector2f Vel;
		float Age = 0.f;
		float Life = 1.f;
		FLinearColor Color;
	};

	struct FFloatLabel
	{
		FString Text;
		float Age = 0.f;
		FLinearColor Color;
	};

	EActiveTimerReturnType Animate(double InCurrentTime, float InDeltaTime);
	void HandleUpgradeBought(const FString& UpgradeId);
	int32 GetStageId() const;
	bool IsReducedMotion() const;
	FPalette GetPalette() const;
	FUpgradeVisuals ComputeUpgradeVisuals() const;
	static int32 CrystalsForCount(int32 Owned);
	/** Point on orbit ring RingIndex at Angle, in model space. */
	FVector3f RingPoint(int32 RingIndex, float Angle) const;

	/** Builds this frame's orbiting crystals and crown shards as extra geometry in model space. */
	void BuildUpgradeGeometry(const FUpgradeVisuals& Visuals, EvoMesh::FMesh& OutMesh) const;

	/** RevealY: in scanner mode, triangles above this model-space height are drawn as an unscanned ghost. */
	void PaintMeshes(const EvoMesh::FMesh* Meshes[], int32 NumMeshes, float Scale, float ExtraYaw, const FVector2f& Center, float PixelScale,
		const FPalette& Palette, float AccentBoost, const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 Layer,
		float RevealY = TNumericLimits<float>::Max()) const;

	int32 PaintScanner(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	float GetScanTarget() const;

	void PaintOrbitPaths(const FUpgradeVisuals& Visuals, const FVector2f& Center, float PixelScale, const FPalette& Palette,
		const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 Layer) const;

	void PaintPedestal(const FVector2f& Center, float Radius, int32 MultiplierLevels, const FPalette& Palette,
		const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 Layer) const;

	void PaintEffects(const FVector2f& Center, float Radius, float LabelY, const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 Layer) const;

	/** Applies the model-space -> view-space rotation used for the model. */
	FVector3f ToView(const FVector3f& P, float Yaw) const;

	TWeakObjectPtr<UEvolutionGameSubsystem> Subsystem;
	FDelegateHandle UpgradeBoughtHandle;

	TSharedPtr<EvoMesh::FMesh> CurrentMesh;
	TSharedPtr<EvoMesh::FMesh> PreviousMesh;
	int32 CurrentStageId = 0;
	bool bScannerMode = false;
	float ScanProgress = 0.f; // smoothed 0..1 progress toward the next stage's price (scanner mode)
	float MeshMinY = -1.f;   // vertical extent of the current model, in model units
	float MeshMaxY = 1.f;

	float Time = 0.f;
	float Transition = 1.f; // 0 -> 1 while morphing from PreviousMesh to CurrentMesh

	// Purchase feedback
	float Flash = 0.f;
	float Shock = 1.f; // 0 -> 1 while the purchase shockwave expands
	FLinearColor FlashColor = FLinearColor::White;
	TArray<FSpark> Sparks;
	TArray<FFloatLabel> Labels;

	// Grow-in animation for newly added crystals: upgrade id -> seconds since its newest crystal appeared.
	TMap<FString, float> CrystalAge;
	TMap<FString, int32> KnownCrystals;
	int32 KnownClickLevels = -1;
	float CrownAge = 10.f;

	const FSlateBrush* WhiteBrush = nullptr;
	FSlateRoundedBoxBrush DotBrush;

	// Scratch buffers reused every frame to avoid allocations.
	mutable EvoMesh::FMesh UpgradeMesh;
	mutable TArray<FVector3f> ViewPositions;
	mutable TArray<FVector3f> ViewNormals;
	mutable TArray<FSortedTri> SortedTris;
	mutable TArray<FSlateVertex> Vertices;
	mutable TArray<SlateIndex> Indices;
};
