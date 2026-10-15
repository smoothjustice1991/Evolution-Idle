#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/RenderingCommon.h"
#include "UI/EvoStageMeshes.h"

class UEvolutionGameSubsystem;
struct FSlateBrush;

/**
 * Background "hologram" of the current evolution stage: a low-poly 3D model that floats, slowly
 * turns and sways behind the helix. Models are generated in EvoStageMeshes.cpp, shaded here on the
 * CPU (flat lighting, rim light, depth fade) and drawn as one batch of Slate triangles.
 * When the stage changes, the old model shrinks away while the new one grows in.
 */
class SEvoStageModel : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SEvoStageModel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UEvolutionGameSubsystem>, Subsystem)
	SLATE_END_ARGS()

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
		int32 Index = 0;
		FVector3f Normal = FVector3f::ZeroVector;
	};

	EActiveTimerReturnType Animate(double InCurrentTime, float InDeltaTime);
	int32 GetStageId() const;
	bool IsReducedMotion() const;
	FPalette GetPalette() const;

	void PaintMesh(const EvoMesh::FMesh& Mesh, float Scale, float ExtraYaw, const FVector2f& Center, float Radius,
		const FPalette& Palette, const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 Layer) const;

	void PaintPedestal(const FVector2f& Center, float Radius, float Alpha, const FPalette& Palette,
		const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 Layer) const;

	TWeakObjectPtr<UEvolutionGameSubsystem> Subsystem;

	TSharedPtr<EvoMesh::FMesh> CurrentMesh;
	TSharedPtr<EvoMesh::FMesh> PreviousMesh;
	int32 CurrentStageId = 0;

	float Time = 0.f;
	float Transition = 1.f; // 0 -> 1 while morphing from PreviousMesh to CurrentMesh

	const FSlateBrush* WhiteBrush = nullptr;

	// Scratch buffers reused every frame to avoid allocations.
	mutable TArray<FVector3f> ViewPositions;
	mutable TArray<FSortedTri> SortedTris;
	mutable TArray<FSlateVertex> Vertices;
	mutable TArray<SlateIndex> Indices;
};
