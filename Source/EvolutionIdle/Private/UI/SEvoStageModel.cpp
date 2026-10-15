#include "UI/SEvoStageModel.h"
#include "EvolutionGameSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Textures/SlateShaderResource.h"

namespace
{
	// Keep these in sync with Tools/stage_preview/render.py so previews match the game.
	constexpr float StageCameraDistance = 4.f;   // in model units (models fit in a unit sphere)
	constexpr float StageViewPitch = 0.22f;      // look slightly down on the model
	constexpr float StageSpinSpeed = 0.35f;      // radians per second
	constexpr float StageVisibility = 0.9f;      // blend toward the background so the model stays "in the background"
	constexpr float StageTransitionSeconds = 1.3f;

	FLinearColor StageLerp3(const FLinearColor& A, const FLinearColor& B, float T)
	{
		return FLinearColor(A.R + (B.R - A.R) * T, A.G + (B.G - A.G) * T, A.B + (B.B - A.B) * T, 1.f);
	}

	FLinearColor StageScale3(const FLinearColor& C, float S)
	{
		return FLinearColor(C.R * S, C.G * S, C.B * S, 1.f);
	}

	float StageEaseOutBack(float T)
	{
		const float C1 = 1.4f;
		const float C3 = C1 + 1.f;
		const float U = T - 1.f;
		return 1.f + C3 * U * U * U + C1 * U * U;
	}
}

void SEvoStageModel::Construct(const FArguments& InArgs)
{
	Subsystem = InArgs._Subsystem;
	WhiteBrush = FCoreStyle::Get().GetBrush("GenericWhiteBox");
	SetVisibility(EVisibility::HitTestInvisible);
	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SEvoStageModel::Animate));
}

FVector2D SEvoStageModel::ComputeDesiredSize(float) const
{
	return FVector2D(200.0, 200.0);
}

int32 SEvoStageModel::GetStageId() const
{
	const UEvolutionGameSubsystem* Sub = Subsystem.Get();
	if (!Sub || !Sub->HasGameData())
	{
		return 0;
	}
	const FEvoStageDef* Stage = Sub->GetEconomy().GetCurrentStage();
	return Stage ? Stage->Id : 0;
}

bool SEvoStageModel::IsReducedMotion() const
{
	const UEvolutionGameSubsystem* Sub = Subsystem.Get();
	return Sub && Sub->GetReducedMotion();
}

SEvoStageModel::FPalette SEvoStageModel::GetPalette() const
{
	FPalette Palette;
	Palette.Primary = FLinearColor(0.02f, 0.75f, 1.0f);
	Palette.Secondary = FLinearColor(0.01f, 1.0f, 0.45f);
	Palette.Background = FLinearColor(0.002f, 0.004f, 0.008f);
	if (const UEvolutionGameSubsystem* Sub = Subsystem.Get())
	{
		if (Sub->HasGameData())
		{
			if (const FEvoEraDef* Era = Sub->GetEconomy().GetCurrentEra())
			{
				Palette.Primary = Era->GetPrimary();
				Palette.Secondary = Era->GetSecondary();
				Palette.Background = Era->GetBackground();
			}
		}
	}
	return Palette;
}

EActiveTimerReturnType SEvoStageModel::Animate(double InCurrentTime, float InDeltaTime)
{
	const float Dt = FMath::Clamp(InDeltaTime, 0.f, 0.1f);
	const bool bReduced = IsReducedMotion();
	Time += Dt * (bReduced ? 0.3f : 1.f);

	const int32 StageId = GetStageId();
	if (StageId > 0 && StageId != CurrentStageId)
	{
		const bool bFirst = CurrentStageId == 0;
		PreviousMesh = bFirst ? nullptr : CurrentMesh;
		CurrentMesh = MakeShared<EvoMesh::FMesh>(EvoMesh::BuildStageMesh(StageId));
		CurrentStageId = StageId;
		Transition = (bFirst || bReduced) ? 1.f : 0.f;
	}

	if (Transition < 1.f)
	{
		Transition = FMath::Min(1.f, Transition + Dt / StageTransitionSeconds);
		if (Transition >= 1.f)
		{
			PreviousMesh.Reset();
		}
	}

	Invalidate(EInvalidateWidgetReason::Paint);
	return EActiveTimerReturnType::Continue;
}

int32 SEvoStageModel::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize());
	if (Size.X < 40.f || Size.Y < 40.f || !CurrentMesh.IsValid())
	{
		return LayerId;
	}

	const bool bReduced = IsReducedMotion();
	const FPalette Palette = GetPalette();

	// Float on the left of the evolution area so the helix in the middle stays readable.
	const float Radius = FMath::Min(Size.Y * 0.4f, Size.X * 0.22f);
	const float Bob = bReduced ? 0.f : FMath::Sin(Time * 0.9f) * Radius * 0.05f;
	const FVector2f Center(FMath::Max(Radius * 1.05f, Size.X * 0.24f), Size.Y * 0.5f + Bob);

	const int32 PedestalLayer = LayerId;
	const int32 ModelLayer = LayerId + 1;

	if (Transition < 1.f)
	{
		const float Out = FMath::Clamp(Transition * 1.6f, 0.f, 1.f);
		const float In = FMath::Clamp((Transition - 0.25f) / 0.75f, 0.f, 1.f);
		if (PreviousMesh.IsValid() && Out < 1.f)
		{
			const float Shrink = 1.f - Out * Out;
			PaintMesh(*PreviousMesh, Shrink, Out * 4.f, Center, Radius, Palette, AllottedGeometry, OutDrawElements, ModelLayer);
		}
		if (In > 0.f)
		{
			PaintMesh(*CurrentMesh, FMath::Max(0.01f, StageEaseOutBack(In)), (1.f - In) * -3.f, Center, Radius, Palette, AllottedGeometry, OutDrawElements, ModelLayer);
		}
	}
	else
	{
		PaintMesh(*CurrentMesh, 1.f, 0.f, Center, Radius, Palette, AllottedGeometry, OutDrawElements, ModelLayer);
	}

	PaintPedestal(FVector2f(Center.X, Size.Y * 0.5f + Radius * 1.02f), Radius, 1.f, Palette, AllottedGeometry, OutDrawElements, PedestalLayer);
	return ModelLayer;
}

void SEvoStageModel::PaintPedestal(const FVector2f& Center, float Radius, float Alpha, const FPalette& Palette,
	const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 Layer) const
{
	const float Pulse = 0.5f + 0.5f * FMath::Sin(Time * 1.6f);
	for (int32 Ring = 0; Ring < 3; ++Ring)
	{
		const float Rx = Radius * (0.55f + 0.2f * Ring);
		const float Ry = Rx * 0.18f;
		TArray<FVector2f> Points;
		constexpr int32 Segments = 64;
		Points.Reserve(Segments + 1);
		for (int32 I = 0; I <= Segments; ++I)
		{
			const float A = 2.f * UE_PI * static_cast<float>(I) / static_cast<float>(Segments);
			Points.Add(Center + FVector2f(FMath::Cos(A) * Rx, FMath::Sin(A) * Ry));
		}
		FLinearColor Color = Ring == 1 ? Palette.Secondary : Palette.Primary;
		Color.A = Alpha * (0.32f - 0.08f * Ring) * (0.7f + 0.3f * Pulse);
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, 1.5f);
	}
}

void SEvoStageModel::PaintMesh(const EvoMesh::FMesh& Mesh, float Scale, float ExtraYaw, const FVector2f& Center, float Radius,
	const FPalette& Palette, const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 Layer) const
{
	const int32 NumVerts = static_cast<int32>(Mesh.Positions.size());
	const int32 NumTris = Mesh.NumTriangles();
	if (NumVerts == 0 || NumTris == 0 || Scale <= 0.f)
	{
		return;
	}

	const bool bReduced = IsReducedMotion();

	// ---- Transform to view space ----
	const float Yaw = Time * StageSpinSpeed + ExtraYaw;
	const float CosY = FMath::Cos(Yaw), SinY = FMath::Sin(Yaw);
	const float CosP = FMath::Cos(StageViewPitch), SinP = FMath::Sin(StageViewPitch);

	ViewPositions.SetNumUninitialized(NumVerts, EAllowShrinking::No);
	for (int32 I = 0; I < NumVerts; ++I)
	{
		const EvoMesh::FV3& Src = Mesh.Positions[I];
		float X = Src.X, Y = Src.Y, Z = Src.Z;
		const float Flex = Mesh.Flex[I];
		if (Flex > 0.f && !bReduced)
		{
			const float Amp = Flex * 0.12f;
			X += Amp * 0.4f * FMath::Sin(Time * 1.7f + Src.Y * 3.f);
			Z += Amp * FMath::Sin(Time * 2.2f + Src.X * 2.5f);
		}
		const float X1 = X * CosY + Z * SinY;
		const float Z1 = -X * SinY + Z * CosY;
		const float Y2 = Y * CosP - Z1 * SinP;
		const float Z2 = Y * SinP + Z1 * CosP;
		ViewPositions[I] = FVector3f(X1, Y2, Z2) * Scale;
	}

	// ---- Cull back faces and sort far to near ----
	SortedTris.Reset();
	for (int32 T = 0; T < NumTris; ++T)
	{
		const FVector3f& A = ViewPositions[Mesh.Indices[T * 3]];
		const FVector3f& B = ViewPositions[Mesh.Indices[T * 3 + 1]];
		const FVector3f& C = ViewPositions[Mesh.Indices[T * 3 + 2]];
		FVector3f N = FVector3f::CrossProduct(B - A, C - A);
		const FVector3f Centroid = (A + B + C) / 3.f;
		const FVector3f ToCamera(-Centroid.X, -Centroid.Y, StageCameraDistance - Centroid.Z);
		if (FVector3f::DotProduct(N, ToCamera) <= 0.f)
		{
			if (!Mesh.TriTwoSided[T])
			{
				continue;
			}
			N = -N;
		}
		if (!N.Normalize(1e-12f))
		{
			continue;
		}
		FSortedTri& Entry = SortedTris.AddDefaulted_GetRef();
		Entry.Depth = Centroid.Z;
		Entry.Index = T;
		Entry.Normal = N;
	}
	SortedTris.Sort([](const FSortedTri& L, const FSortedTri& R) { return L.Depth < R.Depth; });

	// ---- Shade ----
	const FLinearColor White(1.f, 1.f, 1.f);
	FLinearColor SlotColors[EvoMesh::SlotCount];
	SlotColors[EvoMesh::SlotPrimary] = StageScale3(Palette.Primary, 0.85f);
	SlotColors[EvoMesh::SlotSecondary] = StageScale3(Palette.Secondary, 0.85f);
	SlotColors[EvoMesh::SlotAccent] = StageLerp3(Palette.Primary, White, 0.55f);
	SlotColors[EvoMesh::SlotDark] = StageLerp3(Palette.Background, Palette.Primary, 0.22f);
	SlotColors[EvoMesh::SlotMid] = StageScale3(StageLerp3(Palette.Primary, Palette.Secondary, 0.5f), 0.55f);
	const FLinearColor RimColor = StageLerp3(Palette.Secondary, White, 0.3f);

	const FVector3f Light = FVector3f(-0.45f, 0.7f, 0.55f).GetSafeNormal();
	const FVector3f Half = (Light + FVector3f(0.f, 0.f, 1.f)).GetSafeNormal();

	const FSlateRenderTransform& RenderTransform = Geometry.GetAccumulatedRenderTransform();
	FSlateResourceHandle Handle;
	FVector2f UV(0.5f, 0.5f);
	if (WhiteBrush)
	{
		Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*WhiteBrush);
		if (const FSlateShaderResourceProxy* Proxy = Handle.GetResourceProxy())
		{
			UV = Proxy->StartUV + Proxy->SizeUV * 0.5f;
		}
	}

	const float PixelScale = Radius;
	Vertices.Reset();
	Indices.Reset();
	Vertices.Reserve(SortedTris.Num() * 3);
	Indices.Reserve(SortedTris.Num() * 3);

	for (const FSortedTri& Entry : SortedTris)
	{
		const uint8 Slot = FMath::Min<uint8>(Mesh.TriSlot[Entry.Index], EvoMesh::SlotCount - 1);
		const FLinearColor& Base = SlotColors[Slot];
		const FVector3f& N = Entry.Normal;
		const float Diffuse = FMath::Max(0.f, FVector3f::DotProduct(N, Light));

		FLinearColor Color;
		if (Slot == EvoMesh::SlotAccent)
		{
			Color = StageScale3(Base, 0.85f + 0.3f * Diffuse);
		}
		else
		{
			const float Rim = FMath::Pow(1.f - FMath::Max(0.f, N.Z), 3.f);
			const float Spec = FMath::Pow(FMath::Max(0.f, FVector3f::DotProduct(N, Half)), 24.f) * 0.35f;
			const float Lit = 0.22f + 0.78f * Diffuse;
			Color = FLinearColor(
				Base.R * Lit + RimColor.R * Rim * 0.55f + Spec,
				Base.G * Lit + RimColor.G * Rim * 0.55f + Spec,
				Base.B * Lit + RimColor.B * Rim * 0.55f + Spec, 1.f);
			const float Fog = FMath::Clamp(0.5f + 0.5f * Entry.Depth / FMath::Max(Scale, 0.01f), 0.f, 1.f);
			Color = StageScale3(Color, 0.6f + 0.4f * Fog);
		}
		Color = StageLerp3(Palette.Background, Color, StageVisibility);
		const FColor VertexColor = Color.ToFColor(true);

		const SlateIndex BaseIndex = static_cast<SlateIndex>(Vertices.Num());
		for (int32 K = 0; K < 3; ++K)
		{
			const FVector3f& P = ViewPositions[Mesh.Indices[Entry.Index * 3 + K]];
			const float Perspective = StageCameraDistance / FMath::Max(0.5f, StageCameraDistance - P.Z);
			const FVector2f Local = Center + FVector2f(P.X * Perspective, -P.Y * Perspective) * PixelScale;
			Vertices.Add(FSlateVertex::Make(RenderTransform, Local, UV, VertexColor));
			Indices.Add(BaseIndex + K);
		}
	}

	if (Vertices.Num() > 0)
	{
		FSlateDrawElement::MakeCustomVerts(OutDrawElements, Layer, Handle, Vertices, Indices, nullptr, 0, 0);
	}
}
