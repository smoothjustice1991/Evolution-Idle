#include "UI/SEvoStageModel.h"
#include "EvolutionGameSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
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

	// Upgrade indicators
	constexpr int32 StageMaxOrbitRings = 4;
	constexpr int32 StageMaxCrownShards = 10;
	constexpr int32 StageMaxHaloRings = 5;
	constexpr float StageCrystalGrowSeconds = 0.7f;
	constexpr float StageFitScale = 0.78f;      // model size relative to the old radius-based layout (vertical limit)
	constexpr float StageExtent = 1.62f;        // furthest horizontal reach of orbits / pedestal halos, in model units

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

	EvoMesh::FV3 StageToMesh(const FVector3f& V)
	{
		return EvoMesh::FV3(V.X, V.Y, V.Z);
	}
}

SEvoStageModel::SEvoStageModel()
	: DotBrush(FLinearColor::White, 4.f)
{
}

SEvoStageModel::~SEvoStageModel()
{
	if (UEvolutionGameSubsystem* Sub = Subsystem.Get())
	{
		Sub->OnUpgradeBought.Remove(UpgradeBoughtHandle);
	}
}

void SEvoStageModel::Construct(const FArguments& InArgs)
{
	Subsystem = InArgs._Subsystem;
	bScannerMode = InArgs._ScannerMode;
	WhiteBrush = FCoreStyle::Get().GetBrush("GenericWhiteBox");
	SetVisibility(EVisibility::HitTestInvisible);

	UEvolutionGameSubsystem* SubForEvents = bScannerMode ? nullptr : Subsystem.Get();
	if (UEvolutionGameSubsystem* Sub = SubForEvents)
	{
		UpgradeBoughtHandle = Sub->OnUpgradeBought.AddSP(this, &SEvoStageModel::HandleUpgradeBought);
	}

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
	const FEvoEconomy& Econ = Sub->GetEconomy();
	const FEvoStageDef* Stage = Econ.GetCurrentStage();
	if (bScannerMode && Econ.GetNextStage())
	{
		Stage = Econ.GetNextStage();
	}
	return Stage ? Stage->Id : 0;
}

float SEvoStageModel::GetScanTarget() const
{
	const UEvolutionGameSubsystem* Sub = Subsystem.Get();
	if (!Sub || !Sub->HasGameData())
	{
		return 0.f;
	}
	const FEvoEconomy& Econ = Sub->GetEconomy();
	const FEvoStageDef* Next = Econ.GetNextStage();
	if (!Next || Next->Cost <= 0.0)
	{
		return 1.f;
	}
	return static_cast<float>(FMath::Clamp(Econ.GetState().CurrentEP / Next->Cost, 0.0, 1.0));
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

// ---------------------------------------------------------------------------
// Upgrade bookkeeping
// ---------------------------------------------------------------------------

int32 SEvoStageModel::CrystalsForCount(int32 Owned)
{
	if (Owned <= 0)
	{
		return 0;
	}
	// 1 -> 1, 2-3 -> 2, 4-7 -> 3, 8-15 -> 4, 16-31 -> 5, 32+ -> 6
	return FMath::Min(6, 1 + static_cast<int32>(FMath::FloorLog2(static_cast<uint32>(Owned))));
}

SEvoStageModel::FUpgradeVisuals SEvoStageModel::ComputeUpgradeVisuals() const
{
	FUpgradeVisuals Visuals;
	const UEvolutionGameSubsystem* Sub = Subsystem.Get();
	if (!Sub || !Sub->HasGameData())
	{
		return Visuals;
	}
	const FEvoEconomy& Econ = Sub->GetEconomy();

	TArray<FOrbitRing> Owned;
	for (const FEvoUpgradeDef& Def : Econ.GetData().Upgrades)
	{
		const int32 Level = Econ.GetLevel(Def.Id);
		if (Level <= 0)
		{
			continue;
		}
		switch (Def.GetEffect())
		{
		case EEvoEffect::Production:
		{
			FOrbitRing& Ring = Owned.AddDefaulted_GetRef();
			Ring.UpgradeId = Def.Id;
			Ring.Crystals = CrystalsForCount(Level);
			break;
		}
		case EEvoEffect::ClickPower:
			Visuals.ClickLevels += Level;
			break;
		case EEvoEffect::Multiplier:
			Visuals.MultiplierLevels += Level;
			break;
		default:
			break;
		}
	}

	// Show the newest auto-clicker types (data order runs from earliest to latest era).
	const int32 First = FMath::Max(0, Owned.Num() - StageMaxOrbitRings);
	for (int32 Index = First; Index < Owned.Num(); ++Index)
	{
		FOrbitRing Ring = Owned[Index];
		Ring.RingIndex = Index - First;
		Visuals.Rings.Add(Ring);
	}
	return Visuals;
}

void SEvoStageModel::HandleUpgradeBought(const FString& UpgradeId)
{
	const UEvolutionGameSubsystem* Sub = Subsystem.Get();
	if (!Sub || !Sub->HasGameData())
	{
		return;
	}
	const FEvoUpgradeDef* Def = Sub->GetEconomy().FindUpgrade(UpgradeId);
	if (!Def)
	{
		return;
	}

	const FPalette Palette = GetPalette();
	switch (Def->GetEffect())
	{
	case EEvoEffect::Production: FlashColor = Palette.Secondary; break;
	case EEvoEffect::ClickPower: FlashColor = StageLerp3(Palette.Primary, FLinearColor::White, 0.6f); break;
	default:                     FlashColor = StageLerp3(Palette.Primary, FLinearColor(1.f, 0.85f, 0.4f), 0.5f); break;
	}
	Flash = 1.f;
	Shock = 0.f;

	if (Labels.Num() >= 6)
	{
		Labels.RemoveAt(0);
	}
	FFloatLabel& Label = Labels.AddDefaulted_GetRef();
	Label.Text = FString::Printf(TEXT("+1 %s"), *Def->Name);
	Label.Color = FlashColor;

	if (!IsReducedMotion())
	{
		for (int32 Index = 0; Index < 26 && Sparks.Num() < 150; ++Index)
		{
			FSpark& Spark = Sparks.AddDefaulted_GetRef();
			const float Angle = FMath::FRandRange(0.f, 2.f * UE_PI);
			const float Speed = FMath::FRandRange(0.6f, 1.6f);
			Spark.Pos = FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * 0.25f;
			Spark.Vel = FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Speed;
			Spark.Life = FMath::FRandRange(0.8f, 1.4f);
			Spark.Color = StageLerp3(FlashColor, FLinearColor::White, FMath::FRandRange(0.f, 0.5f));
		}
	}
}

// ---------------------------------------------------------------------------
// Animation
// ---------------------------------------------------------------------------

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
		MeshMinY = 1.f;
		MeshMaxY = -1.f;
		for (const EvoMesh::FV3& P : CurrentMesh->Positions)
		{
			MeshMinY = FMath::Min(MeshMinY, P.Y);
			MeshMaxY = FMath::Max(MeshMaxY, P.Y);
		}
		Transition = (bFirst || bReduced) ? 1.f : 0.f;
		ScanProgress = GetScanTarget();
	}

	if (bScannerMode)
	{
		const float Target = GetScanTarget();
		ScanProgress += (Target - ScanProgress) * FMath::Min(1.f, Dt * 5.f);
	}

	if (Transition < 1.f)
	{
		Transition = FMath::Min(1.f, Transition + Dt / StageTransitionSeconds);
		if (Transition >= 1.f)
		{
			PreviousMesh.Reset();
		}
	}

	// Track crystal counts so newly earned crystals grow in.
	const FUpgradeVisuals Visuals = ComputeUpgradeVisuals();
	const bool bFirstCount = KnownClickLevels < 0;
	for (const FOrbitRing& Ring : Visuals.Rings)
	{
		int32& Known = KnownCrystals.FindOrAdd(Ring.UpgradeId);
		if (Ring.Crystals > Known && !bFirstCount)
		{
			CrystalAge.Add(Ring.UpgradeId, 0.f);
		}
		Known = Ring.Crystals;
	}
	if (Visuals.ClickLevels > KnownClickLevels && !bFirstCount)
	{
		CrownAge = 0.f;
	}
	KnownClickLevels = Visuals.ClickLevels;
	if (Visuals.Rings.Num() == 0)
	{
		KnownCrystals.Reset(); // progress was reset
	}
	for (TPair<FString, float>& Pair : CrystalAge)
	{
		Pair.Value += Dt;
	}
	CrownAge += Dt;

	// Purchase effects
	Flash = FMath::Max(0.f, Flash - Dt * 1.1f);
	Shock = FMath::Min(1.f, Shock + Dt * 1.4f);
	for (int32 Index = Sparks.Num() - 1; Index >= 0; --Index)
	{
		FSpark& Spark = Sparks[Index];
		Spark.Age += Dt;
		Spark.Pos += Spark.Vel * Dt;
		Spark.Vel *= FMath::Max(0.f, 1.f - 2.5f * Dt);
		if (Spark.Age >= Spark.Life)
		{
			Sparks.RemoveAtSwap(Index);
		}
	}
	for (int32 Index = Labels.Num() - 1; Index >= 0; --Index)
	{
		Labels[Index].Age += Dt;
		if (Labels[Index].Age > 1.6f)
		{
			Labels.RemoveAt(Index);
		}
	}

	Invalidate(EInvalidateWidgetReason::Paint);
	return EActiveTimerReturnType::Continue;
}

// ---------------------------------------------------------------------------
// Geometry helpers
// ---------------------------------------------------------------------------

FVector3f SEvoStageModel::RingPoint(int32 RingIndex, float Angle) const
{
	const float Radius = 1.12f + 0.11f * static_cast<float>(RingIndex);
	const float Pitch = 0.32f + 0.28f * FMath::Sin(static_cast<float>(RingIndex) * 1.9f + 0.4f);
	const float Roll = 0.35f * FMath::Cos(static_cast<float>(RingIndex) * 2.3f);
	FVector3f P(FMath::Cos(Angle) * Radius, 0.f, FMath::Sin(Angle) * Radius);
	// roll (Z) then pitch (X)
	P = FVector3f(P.X * FMath::Cos(Roll) - P.Y * FMath::Sin(Roll), P.X * FMath::Sin(Roll) + P.Y * FMath::Cos(Roll), P.Z);
	P = FVector3f(P.X, P.Y * FMath::Cos(Pitch) - P.Z * FMath::Sin(Pitch), P.Y * FMath::Sin(Pitch) + P.Z * FMath::Cos(Pitch));
	return P;
}

FVector3f SEvoStageModel::ToView(const FVector3f& P, float Yaw) const
{
	const float CosY = FMath::Cos(Yaw), SinY = FMath::Sin(Yaw);
	const float CosP = FMath::Cos(StageViewPitch), SinP = FMath::Sin(StageViewPitch);
	const float X1 = P.X * CosY + P.Z * SinY;
	const float Z1 = -P.X * SinY + P.Z * CosY;
	return FVector3f(X1, P.Y * CosP - Z1 * SinP, P.Y * SinP + Z1 * CosP);
}

void SEvoStageModel::BuildUpgradeGeometry(const FUpgradeVisuals& Visuals, EvoMesh::FMesh& OutMesh) const
{
	OutMesh.Positions.clear();
	OutMesh.Flex.clear();
	OutMesh.Indices.clear();
	OutMesh.TriSlot.clear();
	OutMesh.TriTwoSided.clear();

	const bool bReduced = IsReducedMotion();
	static const uint8 RingSlots[StageMaxOrbitRings] = { EvoMesh::SlotPrimary, EvoMesh::SlotSecondary, EvoMesh::SlotPrimary, EvoMesh::SlotAccent };

	// Orbiting crystals, one ring per auto-clicker type.
	for (const FOrbitRing& Ring : Visuals.Rings)
	{
		const float Speed = (0.9f / (1.f + 0.35f * static_cast<float>(Ring.RingIndex))) * (bReduced ? 0.3f : 1.f);
		const float* AgePtr = CrystalAge.Find(Ring.UpgradeId);
		for (int32 Index = 0; Index < Ring.Crystals; ++Index)
		{
			float Grow = 1.f;
			if (AgePtr && Index == Ring.Crystals - 1)
			{
				Grow = FMath::Clamp(*AgePtr / StageCrystalGrowSeconds, 0.f, 1.f);
				Grow = Grow <= 0.f ? 0.f : StageEaseOutBack(Grow);
			}
			if (Grow <= 0.01f)
			{
				continue;
			}
			const float Angle = Time * Speed + 2.f * UE_PI * static_cast<float>(Index) / static_cast<float>(Ring.Crystals) + static_cast<float>(Ring.RingIndex) * 0.9f;
			const FVector3f Pos = RingPoint(Ring.RingIndex, Angle);
			const FVector3f Ahead = RingPoint(Ring.RingIndex, Angle + 0.05f);
			const float Length = (0.1f + 0.015f * static_cast<float>(Ring.RingIndex)) * Grow;
			const uint8 Slot = (Index == Ring.Crystals - 1 && AgePtr && *AgePtr < 1.2f) ? static_cast<uint8>(EvoMesh::SlotAccent) : RingSlots[Ring.RingIndex % StageMaxOrbitRings];
			EvoMesh::AppendGem(OutMesh, StageToMesh(Pos), StageToMesh(Ahead - Pos), Length, Length * 0.45f, Slot);
		}
	}

	// Crown of shards above the model for click power.
	const int32 Shards = FMath::Min(Visuals.ClickLevels, StageMaxCrownShards);
	for (int32 Index = 0; Index < Shards; ++Index)
	{
		float Grow = 1.f;
		if (Index == Shards - 1 && CrownAge < StageCrystalGrowSeconds)
		{
			Grow = FMath::Max(0.f, StageEaseOutBack(CrownAge / StageCrystalGrowSeconds));
		}
		if (Grow <= 0.01f)
		{
			continue;
		}
		const float Angle = -Time * 0.6f + 2.f * UE_PI * static_cast<float>(Index) / static_cast<float>(Shards);
		const float Bounce = bReduced ? 0.f : 0.03f * FMath::Sin(Time * 3.f + static_cast<float>(Index));
		const FVector3f Pos(FMath::Cos(Angle) * 0.3f, MeshMaxY + 0.22f + Bounce, FMath::Sin(Angle) * 0.3f);
		EvoMesh::AppendGem(OutMesh, StageToMesh(Pos), EvoMesh::FV3(0.f, 1.f, 0.f), 0.1f * Grow, 0.035f * Grow, EvoMesh::SlotAccent);
	}
}

// ---------------------------------------------------------------------------
// Paint
// ---------------------------------------------------------------------------

int32 SEvoStageModel::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize());
	if (Size.X < 40.f || Size.Y < 40.f || !CurrentMesh.IsValid())
	{
		return LayerId;
	}

	if (bScannerMode)
	{
		return PaintScanner(AllottedGeometry, OutDrawElements, LayerId);
	}

	const bool bReduced = IsReducedMotion();
	const FPalette Palette = GetPalette();
	const FUpgradeVisuals Visuals = ComputeUpgradeVisuals();

	// Float on the left of the evolution area so the helix in the middle stays readable.
	// Centre the model in the gap between the left edge and the helix, and size it (with its orbits,
	// pedestal and halos, which reach about StageExtent model units out) so nothing overlaps the helix.
	// Helix half-width matches SEvoHelix::OnPaint: min(W * 0.2, 100) plus pulse and node glow.
	const float HelixHalfWidth = FMath::Min(Size.X * 0.2f, 100.f) * 1.08f + 14.f;
	const float GapWidth = FMath::Max(40.f, Size.X * 0.5f - HelixHalfWidth);
	const float PixelScale = FMath::Min(Size.Y * 0.4f * StageFitScale, (GapWidth * 0.5f - 6.f) / StageExtent);
	const float Bob = bReduced ? 0.f : FMath::Sin(Time * 0.9f) * PixelScale * 0.06f;
	const FVector2f Center(GapWidth * 0.5f, Size.Y * 0.5f + Bob);

	const int32 BackLayer = LayerId;
	const int32 ModelLayer = LayerId + 1;
	const int32 FxLayer = LayerId + 2;

	const float Pulse = 1.f + 0.07f * Flash * (bReduced ? 0.f : 1.f);
	const float AccentBoost = 1.f + FMath::Min(0.5f, 0.05f * static_cast<float>(Visuals.ClickLevels));

	if (Transition < 1.f)
	{
		const float Out = FMath::Clamp(Transition * 1.6f, 0.f, 1.f);
		const float In = FMath::Clamp((Transition - 0.25f) / 0.75f, 0.f, 1.f);
		if (PreviousMesh.IsValid() && Out < 1.f)
		{
			const EvoMesh::FMesh* Meshes[] = { PreviousMesh.Get() };
			PaintMeshes(Meshes, 1, 1.f - Out * Out, Out * 4.f, Center, PixelScale, Palette, AccentBoost, AllottedGeometry, OutDrawElements, ModelLayer);
		}
		if (In > 0.f)
		{
			const EvoMesh::FMesh* Meshes[] = { CurrentMesh.Get() };
			PaintMeshes(Meshes, 1, FMath::Max(0.01f, StageEaseOutBack(In)), (1.f - In) * -3.f, Center, PixelScale, Palette, AccentBoost, AllottedGeometry, OutDrawElements, ModelLayer);
		}
	}
	else
	{
		BuildUpgradeGeometry(Visuals, UpgradeMesh);
		PaintOrbitPaths(Visuals, Center, PixelScale, Palette, AllottedGeometry, OutDrawElements, BackLayer);
		const EvoMesh::FMesh* Meshes[] = { CurrentMesh.Get(), &UpgradeMesh };
		PaintMeshes(Meshes, 2, Pulse, 0.f, Center, PixelScale, Palette, AccentBoost, AllottedGeometry, OutDrawElements, ModelLayer);
	}

	PaintPedestal(FVector2f(Center.X, Size.Y * 0.5f + PixelScale * (0.25f - MeshMinY)), PixelScale, Visuals.MultiplierLevels, Palette, AllottedGeometry, OutDrawElements, BackLayer);
	PaintEffects(Center, PixelScale, Center.Y - PixelScale * (MeshMaxY + 0.55f), AllottedGeometry, OutDrawElements, FxLayer);
	return FxLayer;
}

void SEvoStageModel::PaintOrbitPaths(const FUpgradeVisuals& Visuals, const FVector2f& Center, float PixelScale, const FPalette& Palette,
	const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 Layer) const
{
	const float Yaw = Time * StageSpinSpeed;
	for (const FOrbitRing& Ring : Visuals.Rings)
	{
		TArray<FVector2f> Points;
		constexpr int32 Segments = 72;
		Points.Reserve(Segments + 1);
		for (int32 Index = 0; Index <= Segments; ++Index)
		{
			const float Angle = 2.f * UE_PI * static_cast<float>(Index) / static_cast<float>(Segments);
			const FVector3f V = ToView(RingPoint(Ring.RingIndex, Angle), Yaw);
			const float Perspective = StageCameraDistance / FMath::Max(0.5f, StageCameraDistance - V.Z);
			Points.Add(Center + FVector2f(V.X * Perspective, -V.Y * Perspective) * PixelScale);
		}
		FLinearColor Color = (Ring.RingIndex % 2) ? Palette.Secondary : Palette.Primary;
		Color.A = 0.16f;
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, 1.f);
	}
}

void SEvoStageModel::PaintPedestal(const FVector2f& Center, float Radius, int32 MultiplierLevels, const FPalette& Palette,
	const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 Layer) const
{
	const float Pulse = 0.5f + 0.5f * FMath::Sin(Time * 1.6f);
	const int32 Halos = FMath::Min(MultiplierLevels, StageMaxHaloRings);
	const FLinearColor Gold = StageLerp3(Palette.Primary, FLinearColor(1.f, 0.85f, 0.4f), 0.55f);

	for (int32 Ring = 0; Ring < 3 + Halos; ++Ring)
	{
		const bool bHalo = Ring >= 3;
		const float Rx = Radius * (0.62f + 0.12f * static_cast<float>(Ring));
		const float Ry = Rx * 0.18f;
		const float Lift = bHalo ? -Radius * 0.012f * static_cast<float>(Ring - 2) : 0.f;
		TArray<FVector2f> Points;
		constexpr int32 Segments = 64;
		Points.Reserve(Segments + 1);
		for (int32 I = 0; I <= Segments; ++I)
		{
			const float A = 2.f * UE_PI * static_cast<float>(I) / static_cast<float>(Segments);
			Points.Add(Center + FVector2f(FMath::Cos(A) * Rx, FMath::Sin(A) * Ry + Lift));
		}
		FLinearColor Color;
		if (bHalo)
		{
			// Multiplier halos shimmer in sequence.
			const float Wave = 0.5f + 0.5f * FMath::Sin(Time * 2.4f - static_cast<float>(Ring) * 0.9f);
			Color = Gold;
			Color.A = (0.3f + 0.35f * Wave) + 0.35f * Flash;
		}
		else
		{
			Color = Ring == 1 ? Palette.Secondary : Palette.Primary;
			Color.A = (0.32f - 0.08f * static_cast<float>(Ring)) * (0.7f + 0.3f * Pulse);
		}
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, bHalo ? 2.f : 1.5f);
	}
}

void SEvoStageModel::PaintEffects(const FVector2f& Center, float Radius, float LabelY, const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 Layer) const
{
	// Shockwave ring expanding from the model
	if (Shock < 1.f)
	{
		const float Ease = 1.f - (1.f - Shock) * (1.f - Shock);
		const float Rx = Radius * (0.35f + 1.1f * Ease);
		const float Ry = Rx * 0.35f;
		TArray<FVector2f> Points;
		constexpr int32 Segments = 64;
		Points.Reserve(Segments + 1);
		for (int32 I = 0; I <= Segments; ++I)
		{
			const float A = 2.f * UE_PI * static_cast<float>(I) / static_cast<float>(Segments);
			Points.Add(Center + FVector2f(FMath::Cos(A) * Rx, FMath::Sin(A) * Ry));
		}
		FLinearColor Color = StageLerp3(FlashColor, FLinearColor::White, 0.3f);
		Color.A = 0.8f * (1.f - Shock);
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, 3.f * (1.f - Shock) + 1.f);
	}

	// Sparks
	for (const FSpark& Spark : Sparks)
	{
		const float Life01 = 1.f - Spark.Age / Spark.Life;
		const float Size = 4.f + 5.f * Life01;
		const FVector2f Pos = Center + Spark.Pos * Radius;
		FLinearColor Color = Spark.Color;
		Color.A = Life01;
		FSlateDrawElement::MakeBox(OutDrawElements, Layer,
			Geometry.ToPaintGeometry(FVector2f(Size, Size), FSlateLayoutTransform(Pos - FVector2f(Size, Size) * 0.5f)),
			&DotBrush, ESlateDrawEffect::None, Color);
	}

	// "+1 Upgrade" labels rising above the model
	if (Labels.Num() > 0)
	{
		const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 15);
		const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		for (int32 Index = 0; Index < Labels.Num(); ++Index)
		{
			const FFloatLabel& Label = Labels[Index];
			const FVector2f TextSize = FVector2f(Measure->Measure(Label.Text, Font));
			const float Rise = Label.Age * Radius * 0.22f;
			const FVector2f Pos = FVector2f(Center.X, LabelY - Rise) - TextSize * 0.5f;
			FLinearColor Color = StageLerp3(Label.Color, FLinearColor::White, 0.4f);
			Color.A = FMath::Clamp(1.6f - Label.Age, 0.f, 1.f) * FMath::Clamp(Label.Age * 6.f, 0.f, 1.f);
			FSlateDrawElement::MakeText(OutDrawElements, Layer, Geometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(Pos)),
				Label.Text, Font, ESlateDrawEffect::None, Color);
		}
	}
}

int32 SEvoStageModel::PaintScanner(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize());
	const FPalette Palette = GetPalette();
	const bool bReduced = IsReducedMotion();

	const int32 BackLayer = LayerId;
	const int32 ModelLayer = LayerId + 1;
	const int32 FxLayer = LayerId + 2;

	auto Line = [&](const FVector2f& A, const FVector2f& B, const FLinearColor& Color, float Thickness, int32 Layer)
	{
		TArray<FVector2f> Points;
		Points.Add(A);
		Points.Add(B);
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, Thickness);
	};

	// Backdrop grid
	FLinearColor GridColor = Palette.Primary;
	GridColor.A = 0.07f;
	constexpr float GridStep = 16.f;
	for (float X = FMath::Fmod(Size.X * 0.5f, GridStep); X < Size.X; X += GridStep)
	{
		Line(FVector2f(X, 0.f), FVector2f(X, Size.Y), GridColor, 1.f, BackLayer);
	}
	for (float Y = FMath::Fmod(Size.Y * 0.5f, GridStep); Y < Size.Y; Y += GridStep)
	{
		Line(FVector2f(0.f, Y), FVector2f(Size.X, Y), GridColor, 1.f, BackLayer);
	}

	// Corner brackets
	FLinearColor Bracket = Palette.Primary;
	Bracket.A = 0.75f;
	const float Len = FMath::Min(16.f, FMath::Min(Size.X, Size.Y) * 0.15f);
	const float Inset = 2.f;
	const FVector2f Corners[4] = { {Inset, Inset}, {Size.X - Inset, Inset}, {Inset, Size.Y - Inset}, {Size.X - Inset, Size.Y - Inset} };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FVector2f C = Corners[Index];
		const float Dx = (Index % 2 == 0) ? Len : -Len;
		const float Dy = (Index < 2) ? Len : -Len;
		Line(C, C + FVector2f(Dx, 0.f), Bracket, 2.f, FxLayer);
		Line(C, C + FVector2f(0.f, Dy), Bracket, 2.f, FxLayer);
	}

	if (!CurrentMesh.IsValid() || Size.X < 20.f || Size.Y < 20.f)
	{
		return FxLayer;
	}

	// Model
	const float PixelScale = FMath::Min(Size.X, Size.Y) * 0.5f / 1.38f;
	const FVector2f Center(Size.X * 0.5f, Size.Y * 0.52f);
	const bool bComplete = ScanProgress >= 0.999f;
	const float Span = FMath::Max(0.01f, MeshMaxY - MeshMinY);
	const float RevealY = bComplete ? TNumericLimits<float>::Max() : MeshMinY + Span * ScanProgress;

	float Scale = 1.f;
	float ExtraYaw = 0.f;
	if (Transition < 1.f)
	{
		Scale = FMath::Max(0.01f, StageEaseOutBack(Transition));
		ExtraYaw = (1.f - Transition) * -3.f;
	}
	const EvoMesh::FMesh* Meshes[] = { CurrentMesh.Get() };
	PaintMeshes(Meshes, 1, Scale, ExtraYaw, Center, PixelScale, Palette, 1.f, AllottedGeometry, OutDrawElements, ModelLayer, RevealY);

	// Scan line at the reveal height (approximate screen position, ignoring perspective)
	if (!bComplete)
	{
		const float LineY = Center.Y - (MeshMinY + Span * ScanProgress) * FMath::Cos(StageViewPitch) * PixelScale * Scale;
		FLinearColor Beam = StageLerp3(Palette.Primary, FLinearColor::White, 0.5f);
		Beam.A = 0.85f;
		Line(FVector2f(6.f, LineY), FVector2f(Size.X - 6.f, LineY), Beam, 1.5f, FxLayer);
		Beam.A = 0.18f;
		Line(FVector2f(6.f, LineY - 2.f), FVector2f(Size.X - 6.f, LineY - 2.f), Beam, 3.f, FxLayer);
		Line(FVector2f(6.f, LineY + 2.f), FVector2f(Size.X - 6.f, LineY + 2.f), Beam, 3.f, FxLayer);
	}

	// Idle sweep beam travelling up and down the frame
	if (!bReduced)
	{
		const float Sweep = 0.5f + 0.5f * FMath::Sin(Time * 1.3f);
		const float SweepY = 6.f + (Size.Y - 12.f) * Sweep;
		FLinearColor SweepColor = Palette.Secondary;
		SweepColor.A = 0.22f;
		Line(FVector2f(4.f, SweepY), FVector2f(Size.X - 4.f, SweepY), SweepColor, 1.f, FxLayer);
	}

	return FxLayer;
}

void SEvoStageModel::PaintMeshes(const EvoMesh::FMesh* Meshes[], int32 NumMeshes, float Scale, float ExtraYaw, const FVector2f& Center, float PixelScale,
	const FPalette& Palette, float AccentBoost, const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 Layer,
	float RevealY) const
{
	if (Scale <= 0.f || NumMeshes <= 0)
	{
		return;
	}

	const bool bReduced = IsReducedMotion();
	const float Yaw = Time * StageSpinSpeed + ExtraYaw;

	// ---- Transform every mesh to view space ----
	int32 TotalVerts = 0;
	for (int32 M = 0; M < NumMeshes; ++M)
	{
		TotalVerts += static_cast<int32>(Meshes[M]->Positions.size());
	}
	ViewPositions.SetNumUninitialized(TotalVerts, EAllowShrinking::No);
	TArray<int32, TInlineAllocator<4>> VertexOffsets;
	int32 Offset = 0;
	for (int32 M = 0; M < NumMeshes; ++M)
	{
		const EvoMesh::FMesh& Mesh = *Meshes[M];
		VertexOffsets.Add(Offset);
		const int32 Count = static_cast<int32>(Mesh.Positions.size());
		for (int32 I = 0; I < Count; ++I)
		{
			const EvoMesh::FV3& Src = Mesh.Positions[I];
			FVector3f P(Src.X, Src.Y, Src.Z);
			const float Flex = Mesh.Flex[I];
			if (Flex > 0.f && !bReduced)
			{
				const float Amp = Flex * 0.12f;
				P.X += Amp * 0.4f * FMath::Sin(Time * 1.7f + Src.Y * 3.f);
				P.Z += Amp * FMath::Sin(Time * 2.2f + Src.X * 2.5f);
			}
			ViewPositions[Offset + I] = ToView(P, Yaw) * Scale;
		}
		Offset += Count;
	}

	// ---- Cull back faces and sort far to near ----
	SortedTris.Reset();
	for (int32 M = 0; M < NumMeshes; ++M)
	{
		const EvoMesh::FMesh& Mesh = *Meshes[M];
		const int32 Base = VertexOffsets[M];
		const int32 NumTris = Mesh.NumTriangles();
		for (int32 T = 0; T < NumTris; ++T)
		{
			const FVector3f& A = ViewPositions[Base + static_cast<int32>(Mesh.Indices[T * 3])];
			const FVector3f& B = ViewPositions[Base + static_cast<int32>(Mesh.Indices[T * 3 + 1])];
			const FVector3f& C = ViewPositions[Base + static_cast<int32>(Mesh.Indices[T * 3 + 2])];
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
			Entry.MeshIndex = M;
			Entry.Index = T;
			Entry.Normal = N;
		}
	}
	SortedTris.Sort([](const FSortedTri& L, const FSortedTri& R) { return L.Depth < R.Depth; });

	// ---- Shade ----
	const FLinearColor White(1.f, 1.f, 1.f);
	FLinearColor SlotColors[EvoMesh::SlotCount];
	SlotColors[EvoMesh::SlotPrimary] = StageScale3(Palette.Primary, 0.85f);
	SlotColors[EvoMesh::SlotSecondary] = StageScale3(Palette.Secondary, 0.85f);
	SlotColors[EvoMesh::SlotAccent] = StageScale3(StageLerp3(Palette.Primary, White, 0.55f), AccentBoost);
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

	Vertices.Reset();
	Indices.Reset();
	Vertices.Reserve(SortedTris.Num() * 3);
	Indices.Reserve(SortedTris.Num() * 3);

	for (const FSortedTri& Entry : SortedTris)
	{
		const EvoMesh::FMesh& Mesh = *Meshes[Entry.MeshIndex];
		const int32 Base = VertexOffsets[Entry.MeshIndex];
		const uint8 Slot = FMath::Min<uint8>(Mesh.TriSlot[Entry.Index], EvoMesh::SlotCount - 1);
		const FLinearColor& BaseColor = SlotColors[Slot];
		const FVector3f& N = Entry.Normal;
		const float Diffuse = FMath::Max(0.f, FVector3f::DotProduct(N, Light));

		FLinearColor Color;
		if (Slot == EvoMesh::SlotAccent)
		{
			Color = StageScale3(BaseColor, 0.85f + 0.3f * Diffuse);
		}
		else
		{
			const float Rim = FMath::Pow(1.f - FMath::Max(0.f, N.Z), 3.f);
			const float Spec = FMath::Pow(FMath::Max(0.f, FVector3f::DotProduct(N, Half)), 24.f) * 0.35f;
			const float Lit = 0.22f + 0.78f * Diffuse;
			Color = FLinearColor(
				BaseColor.R * Lit + RimColor.R * Rim * 0.55f + Spec,
				BaseColor.G * Lit + RimColor.G * Rim * 0.55f + Spec,
				BaseColor.B * Lit + RimColor.B * Rim * 0.55f + Spec, 1.f);
			const float Fog = FMath::Clamp(0.5f + 0.5f * Entry.Depth / FMath::Max(Scale, 0.01f), 0.f, 1.f);
			Color = StageScale3(Color, 0.6f + 0.4f * Fog);
		}
		if (Flash > 0.f)
		{
			// Purchase flash: tint toward the upgrade colour, strongest on edges facing away from the light.
			Color = StageLerp3(Color, FlashColor, Flash * (0.25f + 0.35f * (1.f - Diffuse)));
		}
		if (RevealY < TNumericLimits<float>::Max())
		{
			const float TriY = (Mesh.Positions[Mesh.Indices[Entry.Index * 3]].Y
				+ Mesh.Positions[Mesh.Indices[Entry.Index * 3 + 1]].Y
				+ Mesh.Positions[Mesh.Indices[Entry.Index * 3 + 2]].Y) / 3.f;
			if (TriY > RevealY)
			{
				// Not scanned yet: a dim holographic ghost.
				Color = StageLerp3(Palette.Background, Palette.Primary, 0.12f + 0.2f * Diffuse);
			}
			else if (TriY > RevealY - 0.06f)
			{
				// Bright band where the scan beam is currently writing.
				Color = StageLerp3(Color, White, 0.65f);
			}
		}
		Color = StageLerp3(Palette.Background, Color, StageVisibility);
		const FColor VertexColor = Color.ToFColor(true);

		const SlateIndex BaseIndex = static_cast<SlateIndex>(Vertices.Num());
		for (int32 K = 0; K < 3; ++K)
		{
			const FVector3f& P = ViewPositions[Base + static_cast<int32>(Mesh.Indices[Entry.Index * 3 + K])];
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
