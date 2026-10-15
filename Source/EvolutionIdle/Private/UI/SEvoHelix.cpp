#include "UI/SEvoHelix.h"
#include "UI/EvoFormat.h"
#include "EvolutionGameSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "InputCoreTypes.h"

namespace
{
	constexpr float TwoPi = 2.f * UE_PI;
	constexpr int32 MaxFloaters = 40;
	constexpr int32 MaxParticles = 160;

	FLinearColor WithAlpha(FLinearColor Color, float Alpha)
	{
		Color.A = FMath::Clamp(Alpha, 0.f, 1.f);
		return Color;
	}

	/** Cheap deterministic hash in [0,1) used for the ambient motes. */
	float Hash01(int32 Seed)
	{
		uint32 X = static_cast<uint32>(Seed) * 747796405u + 2891336453u;
		X = ((X >> ((X >> 28u) + 4u)) ^ X) * 277803737u;
		X = (X >> 22u) ^ X;
		return static_cast<float>(X & 0xFFFFFFu) / static_cast<float>(0x1000000u);
	}
}

SEvoHelix::SEvoHelix()
	: SquareBrush(FLinearColor::White)
{
	// Radii 1..40 px in 1 px steps, then coarser steps up to 400 px.
	for (int32 R = 1; R <= 40; ++R) { CircleRadii.Add(static_cast<float>(R)); }
	for (int32 R = 44; R <= 120; R += 4) { CircleRadii.Add(static_cast<float>(R)); }
	for (int32 R = 128; R <= 400; R += 8) { CircleRadii.Add(static_cast<float>(R)); }

	CircleBrushes.Reserve(CircleRadii.Num());
	for (float R : CircleRadii)
	{
		CircleBrushes.Emplace(FLinearColor::White, R);
	}
}

void SEvoHelix::Construct(const FArguments& InArgs)
{
	Subsystem = InArgs._Subsystem;
	Rng.Initialize(0x5EED);
	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SEvoHelix::Animate));
}

bool SEvoHelix::IsReducedMotion() const
{
	const UEvolutionGameSubsystem* Sub = Subsystem.Get();
	return Sub && Sub->GetReducedMotion();
}

int32 SEvoHelix::GetStageId() const
{
	const UEvolutionGameSubsystem* Sub = Subsystem.Get();
	const FEvoStageDef* Stage = Sub ? Sub->GetEconomy().GetCurrentStage() : nullptr;
	return Stage ? Stage->Id : 1;
}

void SEvoHelix::GetPalette(FLinearColor& OutPrimary, FLinearColor& OutSecondary) const
{
	OutPrimary = FLinearColor(0.02f, 0.75f, 1.0f);
	OutSecondary = FLinearColor(0.01f, 1.0f, 0.45f);
	if (const UEvolutionGameSubsystem* Sub = Subsystem.Get())
	{
		if (const FEvoEraDef* Era = Sub->GetEconomy().GetCurrentEra())
		{
			OutPrimary = Era->GetPrimary();
			OutSecondary = Era->GetSecondary();
		}
	}
}

void SEvoHelix::PlayEvolveFlash()
{
	Flash = 1.f;
	PulseTarget = 1.f;
	if (!IsReducedMotion())
	{
		for (int32 Index = 0; Index < 48 && Particles.Num() < MaxParticles; ++Index)
		{
			FParticle& P = Particles.AddDefaulted_GetRef();
			const float Angle = Rng.FRandRange(0.f, TwoPi);
			const float Speed = Rng.FRandRange(120.f, 320.f);
			P.Pos = FVector2f::ZeroVector;
			P.bFromCenter = true;
			P.Vel = FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Speed;
			P.Life = Rng.FRandRange(0.8f, 1.6f);
			P.Radius = Rng.FRandRange(2.f, 5.f);
		}
	}
}

void SEvoHelix::PlayPurchasePulse()
{
	PulseTarget = FMath::Max(PulseTarget, 0.5f);
}

EActiveTimerReturnType SEvoHelix::Animate(double InCurrentTime, float InDeltaTime)
{
	const float Dt = FMath::Clamp(InDeltaTime, 0.f, 0.1f);
	const bool bReduced = IsReducedMotion();

	Time += Dt * (bReduced ? 0.25f : 1.f);
	// The target jumps up on each click and drains away; the displayed pulse eases toward it.
	PulseTarget = FMath::Max(0.f, PulseTarget - Dt * 1.6f);
	Pulse = FMath::FInterpTo(Pulse, PulseTarget, Dt, 12.f);
	Spin += Dt * (bReduced ? 0.25f : 1.f) * (0.9f + 1.2f * Pulse);
	Flash = FMath::Max(0.f, Flash - Dt * (bReduced ? 2.5f : 0.9f));

	for (int32 Index = Floaters.Num() - 1; Index >= 0; --Index)
	{
		FFloatText& F = Floaters[Index];
		F.Age += Dt;
		if (!bReduced)
		{
			F.Pos.Y -= 70.f * Dt;
		}
		if (F.Age > 1.f)
		{
			Floaters.RemoveAt(Index); // keep order: the newest label is always Last()
		}
	}

	for (int32 Index = Particles.Num() - 1; Index >= 0; --Index)
	{
		FParticle& P = Particles[Index];
		P.Age += Dt;
		if (!P.bFromCenter)
		{
			P.Pos += P.Vel * Dt;
			P.Vel *= FMath::Max(0.f, 1.f - 2.2f * Dt);
		}
		if (P.Age >= P.Life || bReduced)
		{
			Particles.RemoveAtSwap(Index);
		}
	}

	Invalidate(EInvalidateWidgetReason::Paint);
	return EActiveTimerReturnType::Continue;
}

FVector2D SEvoHelix::ComputeDesiredSize(float) const
{
	return FVector2D(320.0, 420.0);
}

FCursorReply SEvoHelix::OnCursorQuery(const FGeometry&, const FPointerEvent&) const
{
	return FCursorReply::Cursor(EMouseCursor::Hand);
}

FReply SEvoHelix::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return HandleClick(MyGeometry, MouseEvent);
}

FReply SEvoHelix::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Slate turns the second press of a fast double-click into this event; count it as a normal click.
	return HandleClick(MyGeometry, MouseEvent);
}

FReply SEvoHelix::HandleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	UEvolutionGameSubsystem* Sub = Subsystem.Get();
	if (!Sub || !Sub->HasGameData())
	{
		return FReply::Handled();
	}

	const double Gain = Sub->Click();
	if (Gain <= 0.0)
	{
		return FReply::Handled(); // over the click-rate cap: no EP, no effects
	}
	const FVector2f Local = FVector2f(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
	const bool bReduced = IsReducedMotion();

	const bool bRapid = (Time - LastClickTime) < 0.15f;
	LastClickTime = Time;

	if (!bReduced)
	{
		// Each click pushes the swell up a bit; holding a fast click rate settles into a steady glow.
		PulseTarget = FMath::Min(1.f, PulseTarget + (bRapid ? 0.12f : 0.6f));
	}

	// Rapid clicks add into the newest label for a moment, so the numbers stay readable.
	FFloatText* Latest = Floaters.Num() > 0 ? &Floaters.Last() : nullptr;
	if (Latest && Latest->Age < 0.25f)
	{
		Latest->Value += Gain;
	}
	else
	{
		if (Floaters.Num() >= MaxFloaters)
		{
			Floaters.RemoveAt(0);
		}
		Latest = &Floaters.AddDefaulted_GetRef();
		Latest->Pos = Local + FVector2f(Rng.FRandRange(-12.f, 12.f), -10.f);
		Latest->Value = Gain;
	}
	Latest->Text = FString::Printf(TEXT("+%s EP"), *EvoFormat::Number(Latest->Value));

	if (!bReduced)
	{
		const int32 NumSparks = bRapid ? 2 : 6;
		for (int32 Index = 0; Index < NumSparks && Particles.Num() < MaxParticles; ++Index)
		{
			FParticle& P = Particles.AddDefaulted_GetRef();
			const float Angle = Rng.FRandRange(0.f, TwoPi);
			const float Speed = Rng.FRandRange(60.f, 180.f);
			P.Pos = Local;
			P.Vel = FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Speed;
			P.Life = Rng.FRandRange(0.4f, 0.8f);
			P.Radius = Rng.FRandRange(1.5f, 3.5f);
		}
	}

	return FReply::Handled();
}

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------

void SEvoHelix::DrawCircle(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2f& Center, float Radius, const FLinearColor& Color) const
{
	if (Radius < 0.5f || Color.A <= 0.003f || CircleRadii.Num() == 0)
	{
		return;
	}
	// Pick the brush whose radius is closest to the requested one.
	int32 Best = 0;
	float BestDiff = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index < CircleRadii.Num(); ++Index)
	{
		const float Diff = FMath::Abs(CircleRadii[Index] - Radius);
		if (Diff < BestDiff)
		{
			BestDiff = Diff;
			Best = Index;
		}
		else
		{
			break; // radii are sorted ascending
		}
	}
	const float R = CircleRadii[Best];
	FSlateDrawElement::MakeBox(Out, Layer,
		Geo.ToPaintGeometry(FVector2f(2.f * R, 2.f * R), FSlateLayoutTransform(Center - FVector2f(R, R))),
		&CircleBrushes[Best], ESlateDrawEffect::None, Color);
}

void SEvoHelix::DrawSquare(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2f& Center, float HalfSize, const FLinearColor& Color) const
{
	if (HalfSize <= 0.f || Color.A <= 0.003f)
	{
		return;
	}
	FSlateDrawElement::MakeBox(Out, Layer,
		Geo.ToPaintGeometry(FVector2f(2.f * HalfSize, 2.f * HalfSize), FSlateLayoutTransform(Center - FVector2f(HalfSize, HalfSize))),
		&SquareBrush, ESlateDrawEffect::None, Color);
}

void SEvoHelix::DrawRing(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2f& Center, float RadiusX, float RadiusY, float Angle, const FLinearColor& Color, float Thickness) const
{
	if (Color.A <= 0.003f)
	{
		return;
	}
	TArray<FVector2f> Points;
	constexpr int32 Segments = 72;
	Points.Reserve(Segments + 1);
	const float CosA = FMath::Cos(Angle);
	const float SinA = FMath::Sin(Angle);
	for (int32 Index = 0; Index <= Segments; ++Index)
	{
		const float T = TwoPi * static_cast<float>(Index) / static_cast<float>(Segments);
		const float X = FMath::Cos(T) * RadiusX;
		const float Y = FMath::Sin(T) * RadiusY;
		Points.Add(Center + FVector2f(X * CosA - Y * SinA, X * SinA + Y * CosA));
	}
	FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, Thickness);
}

// ---------------------------------------------------------------------------
// Paint
// ---------------------------------------------------------------------------

int32 SEvoHelix::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize());
	if (Size.X < 8.f || Size.Y < 8.f)
	{
		return LayerId;
	}

	const bool bReduced = IsReducedMotion();
	const int32 StageId = GetStageId();
	FLinearColor Primary, Secondary;
	GetPalette(Primary, Secondary);

	const FVector2f Center(Size.X * 0.5f, Size.Y * 0.5f);
	const float PulseScale = 1.f + (bReduced ? 0.f : 0.08f * Pulse);
	const float MinDim = FMath::Min(Size.X, Size.Y);

	const int32 GlowLayer = LayerId;
	const int32 BackLayer = LayerId + 1;
	const int32 FrontLayer = LayerId + 2;
	const int32 FxLayer = LayerId + 3;
	const int32 TextLayer = LayerId + 4;

	// ---- Ambient motes drifting upward ----
	for (int32 Index = 0; Index < 36; ++Index)
	{
		const float Speed = 0.01f + 0.03f * Hash01(Index * 3 + 1);
		const float X = Hash01(Index * 3) * Size.X;
		const float Y = FMath::Fmod(Hash01(Index * 3 + 2) + 1.f - FMath::Fmod(Time * Speed, 1.f), 1.f) * Size.Y;
		const float Twinkle = 0.35f + 0.35f * FMath::Sin(Time * 1.7f + Index);
		DrawCircle(OutDrawElements, GlowLayer, AllottedGeometry, FVector2f(X, Y), 1.f + 2.f * Hash01(Index + 91),
			WithAlpha(Index % 2 ? Primary : Secondary, 0.25f * Twinkle));
	}

	// ---- Soft central glow ----
	{
		const float GlowR = MinDim * 0.42f * PulseScale;
		for (int32 Ring = 0; Ring < 6; ++Ring)
		{
			const float F = static_cast<float>(Ring) / 6.f;
			DrawCircle(OutDrawElements, GlowLayer, AllottedGeometry, Center, GlowR * (1.f - 0.15f * Ring), WithAlpha(Primary, 0.018f + 0.012f * F));
		}
	}

	if (StageId >= 20)
	{
		// ---- Final form: sphere of pure energy ----
		const float Radius = MinDim * 0.24f * PulseScale * (1.f + 0.03f * FMath::Sin(Time * 2.f));
		constexpr int32 Layers = 20;
		for (int32 Index = 0; Index < Layers; ++Index)
		{
			const float F = static_cast<float>(Index) / static_cast<float>(Layers - 1);
			const float R = FMath::Lerp(Radius * 1.7f, Radius * 0.15f, F);
			FLinearColor Color = F < 0.5f
				? FMath::Lerp(Secondary, Primary, F * 2.f)
				: FMath::Lerp(Primary, FLinearColor::White, (F - 0.5f) * 2.f);
			const float Alpha = F < 0.3f ? 0.04f + 0.1f * F : 0.12f + 0.5f * F;
			DrawCircle(OutDrawElements, F < 0.3f ? GlowLayer : BackLayer, AllottedGeometry, Center, R, WithAlpha(Color, Alpha));
		}
		for (int32 Orbit = 0; Orbit < 3; ++Orbit)
		{
			const float Angle = Time * (0.3f + 0.15f * Orbit) + Orbit * 1.05f;
			const float Rx = Radius * (1.35f + 0.18f * Orbit);
			const float Ry = Rx * (0.28f + 0.1f * FMath::Sin(Time * 0.6f + Orbit));
			DrawRing(OutDrawElements, FrontLayer, AllottedGeometry, Center, Rx, Ry, Angle, WithAlpha(Orbit == 1 ? Secondary : Primary, 0.55f), 1.5f);
			// A spark riding each orbit.
			const float T = Time * (1.2f + 0.4f * Orbit);
			const float SX = FMath::Cos(T) * Rx;
			const float SY = FMath::Sin(T) * Ry;
			const FVector2f Spark = Center + FVector2f(SX * FMath::Cos(Angle) - SY * FMath::Sin(Angle), SX * FMath::Sin(Angle) + SY * FMath::Cos(Angle));
			DrawCircle(OutDrawElements, FrontLayer, AllottedGeometry, Spark, 7.f, WithAlpha(FLinearColor::White, 0.25f));
			DrawCircle(OutDrawElements, FrontLayer, AllottedGeometry, Spark, 3.f, WithAlpha(FLinearColor::White, 0.95f));
		}
	}
	else
	{
		// ---- DNA double helix ----
		const bool bTech = StageId >= 13;
		const bool bEnergyCore = StageId >= 17;
		const float HelixH = FMath::Min(Size.Y * 0.84f, 600.f) * PulseScale;
		const float R = FMath::Min(Size.X * 0.2f, 100.f) * PulseScale;
		const float Top = Center.Y - HelixH * 0.5f;
		const float Turns = 1.7f;
		const float Phase0 = Spin;

		if (bEnergyCore)
		{
			const float CoreScale = 0.45f + 0.15f * static_cast<float>(StageId - 17);
			const float CoreR = R * CoreScale * (1.f + 0.06f * FMath::Sin(Time * 3.f));
			for (int32 Index = 0; Index < 10; ++Index)
			{
				const float F = static_cast<float>(Index) / 9.f;
				DrawCircle(OutDrawElements, BackLayer, AllottedGeometry, Center, FMath::Lerp(CoreR * 2.2f, CoreR * 0.25f, F),
					WithAlpha(FMath::Lerp(Secondary, FLinearColor::White, F), 0.05f + 0.55f * F * F));
			}
		}

		// Strands, split into back/front runs so the far side reads dimmer.
		auto DrawStrand = [&](float PhaseOffset, const FLinearColor& Color)
		{
			constexpr int32 Samples = 96;
			TArray<FVector2f> Run;
			bool bRunFront = false;
			auto Flush = [&]()
			{
				if (Run.Num() >= 2)
				{
					const int32 Layer = bRunFront ? FrontLayer : BackLayer;
					const float Alpha = bRunFront ? 0.95f : 0.35f;
					FSlateDrawElement::MakeLines(OutDrawElements, GlowLayer, AllottedGeometry.ToPaintGeometry(), Run, ESlateDrawEffect::None, WithAlpha(Color, Alpha * 0.18f), true, bTech ? 6.f : 9.f);
					FSlateDrawElement::MakeLines(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(), Run, ESlateDrawEffect::None, WithAlpha(Color, Alpha), true, bTech ? 2.f : 3.f);
				}
				const FVector2f Last = Run.Num() > 0 ? Run.Last() : FVector2f::ZeroVector;
				const bool bHadPoints = Run.Num() > 0;
				Run.Reset();
				if (bHadPoints)
				{
					Run.Add(Last); // keep the strand continuous across the split
				}
			};
			for (int32 S = 0; S <= Samples; ++S)
			{
				const float V = static_cast<float>(S) / static_cast<float>(Samples);
				const float Ph = Phase0 + PhaseOffset + V * TwoPi * Turns;
				const bool bFront = FMath::Cos(Ph) >= 0.f;
				FVector2f P(Center.X + FMath::Sin(Ph) * R, Top + V * HelixH);
				if (bTech)
				{
					// Snap the strand into short circuit-like steps.
					P.X = Center.X + FMath::RoundToFloat(FMath::Sin(Ph) * 6.f) / 6.f * R;
				}
				if (S == 0)
				{
					bRunFront = bFront;
				}
				else if (bFront != bRunFront)
				{
					Flush();
					bRunFront = bFront;
				}
				Run.Add(P);
			}
			Flush();
		};

		DrawStrand(0.f, Primary);
		DrawStrand(UE_PI, Secondary);

		// Rungs (base pairs) and nodes.
		constexpr int32 Rungs = 16;
		for (int32 Index = 0; Index < Rungs; ++Index)
		{
			const float V = (static_cast<float>(Index) + 0.5f) / static_cast<float>(Rungs);
			const float Ph = Phase0 + V * TwoPi * Turns;
			const float Y = Top + V * HelixH;
			const FVector2f A(Center.X + FMath::Sin(Ph) * R, Y);
			const FVector2f B(Center.X + FMath::Sin(Ph + UE_PI) * R, Y);
			const float DepthA = FMath::Cos(Ph);
			const float DepthB = -DepthA;

			TArray<FVector2f> Rung;
			Rung.Add(A);
			Rung.Add(B);
			const FLinearColor RungColor = FMath::Lerp(Primary, Secondary, 0.5f);
			FSlateDrawElement::MakeLines(OutDrawElements, BackLayer, AllottedGeometry.ToPaintGeometry(), Rung, ESlateDrawEffect::None, WithAlpha(RungColor, 0.3f), true, bTech ? 1.f : 2.f);

			auto DrawNode = [&](const FVector2f& P, float Depth, const FLinearColor& Color)
			{
				const float Front01 = (Depth + 1.f) * 0.5f;
				const int32 Layer = Depth >= 0.f ? FrontLayer : BackLayer;
				const float Radius = (bTech ? 4.f : 5.f) * (0.7f + 0.5f * Front01) * PulseScale;
				const float Alpha = 0.35f + 0.65f * Front01;
				if (bTech)
				{
					DrawSquare(OutDrawElements, GlowLayer, AllottedGeometry, P, Radius * 2.f, WithAlpha(Color, 0.12f * Alpha));
					DrawSquare(OutDrawElements, Layer, AllottedGeometry, P, Radius, WithAlpha(Color, Alpha));
				}
				else
				{
					DrawCircle(OutDrawElements, GlowLayer, AllottedGeometry, P, Radius * 2.4f, WithAlpha(Color, 0.12f * Alpha));
					DrawCircle(OutDrawElements, Layer, AllottedGeometry, P, Radius, WithAlpha(FMath::Lerp(Color, FLinearColor::White, 0.25f * Front01), Alpha));
				}
			};
			DrawNode(A, DepthA, Primary);
			DrawNode(B, DepthB, Secondary);

			if (bTech)
			{
				DrawSquare(OutDrawElements, BackLayer, AllottedGeometry, (A + B) * 0.5f, 2.f, WithAlpha(FLinearColor::White, 0.5f));
			}
		}
	}

	// ---- Evolve flash ring ----
	if (Flash > 0.f)
	{
		const float Grow = 1.f - Flash;
		const float RingR = FMath::Lerp(MinDim * 0.1f, MinDim * 0.65f, Grow);
		DrawRing(OutDrawElements, FxLayer, AllottedGeometry, Center, RingR, RingR, 0.f, WithAlpha(FLinearColor::White, Flash), 4.f);
		DrawRing(OutDrawElements, FxLayer, AllottedGeometry, Center, RingR * 0.8f, RingR * 0.8f, 0.f, WithAlpha(Primary, Flash * 0.7f), 2.f);
		DrawCircle(OutDrawElements, GlowLayer, AllottedGeometry, Center, MinDim * 0.45f, WithAlpha(Primary, 0.25f * Flash));
	}

	// ---- Particles ----
	for (const FParticle& P : Particles)
	{
		// Flash particles are spawned without knowing the geometry; they start from the centre.
		const FVector2f Pos = P.bFromCenter ? Center + P.Vel * P.Age : P.Pos;
		const float Life01 = 1.f - P.Age / P.Life;
		DrawCircle(OutDrawElements, FxLayer, AllottedGeometry, Pos, P.Radius, WithAlpha(FMath::Lerp(Primary, FLinearColor::White, 0.5f), Life01));
	}

	// ---- Floating +EP text ----
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 18);
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	for (const FFloatText& F : Floaters)
	{
		const FVector2f TextSize = FVector2f(Measure->Measure(F.Text, Font));
		const float Alpha = 1.f - F.Age;
		FSlateDrawElement::MakeText(OutDrawElements, TextLayer,
			AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(F.Pos - TextSize * 0.5f)),
			F.Text, Font, ESlateDrawEffect::None, WithAlpha(FMath::Lerp(Primary, FLinearColor::White, 0.6f), Alpha));
	}

	return TextLayer;
}
