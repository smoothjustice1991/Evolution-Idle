#pragma once

#include "CoreMinimal.h"

/** Consistent large-number and duration formatting used across the UI. */
namespace EvoFormat
{
	inline FString Number(double Value)
	{
		if (!FMath::IsFinite(Value))
		{
			return TEXT("\u221E");
		}
		const bool bNegative = Value < 0.0;
		double Abs = FMath::Abs(Value);

		FString Result;
		if (Abs < 1000.0)
		{
			// Show one decimal for small fractional values (e.g. EP/sec 2.5), whole numbers otherwise.
			const double Rounded = FMath::FloorToDouble(Abs * 10.0) / 10.0;
			const bool bShowDecimal = Abs < 100.0 && !FMath::IsNearlyEqual(Rounded, FMath::FloorToDouble(Rounded));
			Result = bShowDecimal ? FString::Printf(TEXT("%.1f"), Rounded) : FString::Printf(TEXT("%.0f"), FMath::FloorToDouble(Abs));
		}
		else
		{
			static const TCHAR* Suffixes[] = { TEXT("K"), TEXT("M"), TEXT("B"), TEXT("T"), TEXT("Qa"), TEXT("Qi"), TEXT("Sx"), TEXT("Sp"), TEXT("Oc"), TEXT("No"), TEXT("Dc") };
			int32 Index = -1;
			while (Abs >= 1000.0 && Index < static_cast<int32>(UE_ARRAY_COUNT(Suffixes)) - 1)
			{
				Abs /= 1000.0;
				++Index;
			}
			// Truncate (not round) so a displayed balance is never higher than the real one.
			const double Truncated = FMath::FloorToDouble(Abs * 100.0) / 100.0;
			Result = FString::Printf(TEXT("%.2f%s"), Truncated, Suffixes[Index]);
		}
		return bNegative ? TEXT("-") + Result : Result;
	}

	/** Costs are shown rounded UP so "affordable" in the UI always matches the economy check. */
	inline FString Cost(double Value)
	{
		if (Value < 1000.0 || !FMath::IsFinite(Value))
		{
			return Number(FMath::CeilToDouble(Value));
		}
		static const TCHAR* Suffixes[] = { TEXT("K"), TEXT("M"), TEXT("B"), TEXT("T"), TEXT("Qa"), TEXT("Qi"), TEXT("Sx"), TEXT("Sp"), TEXT("Oc"), TEXT("No"), TEXT("Dc") };
		double Abs = Value;
		int32 Index = -1;
		while (Abs >= 1000.0 && Index < static_cast<int32>(UE_ARRAY_COUNT(Suffixes)) - 1)
		{
			Abs /= 1000.0;
			++Index;
		}
		const double Up = FMath::CeilToDouble(Abs * 100.0 - 1e-9) / 100.0;
		return FString::Printf(TEXT("%.2f%s"), Up, Suffixes[Index]);
	}

	inline FString Duration(double Seconds)
	{
		if (!FMath::IsFinite(Seconds) || Seconds < 0.0)
		{
			return TEXT("\u2014");
		}
		const int64 Total = static_cast<int64>(FMath::CeilToDouble(Seconds));
		const int64 Days = Total / 86400;
		const int64 Hours = (Total % 86400) / 3600;
		const int64 Minutes = (Total % 3600) / 60;
		const int64 Secs = Total % 60;
		if (Days > 0)
		{
			return FString::Printf(TEXT("%lldd %lldh"), Days, Hours);
		}
		if (Hours > 0)
		{
			return FString::Printf(TEXT("%lldh %lldm"), Hours, Minutes);
		}
		if (Minutes > 0)
		{
			return FString::Printf(TEXT("%lldm %llds"), Minutes, Secs);
		}
		return FString::Printf(TEXT("%llds"), Secs);
	}

	inline FString Percent(double Fraction)
	{
		return FString::Printf(TEXT("%.0f%%"), Fraction * 100.0);
	}
}
