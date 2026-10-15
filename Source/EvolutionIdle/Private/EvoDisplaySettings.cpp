#include "EvoDisplaySettings.h"
#include "EvolutionIdle.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameUserSettings.h"
#include "GenericPlatform/GenericApplication.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "EvoDisplay"

namespace EvoDisplay
{
	namespace
	{
		TSharedPtr<SWindow> GetGameWindow()
		{
			return (GEngine && GEngine->GameViewport) ? GEngine->GameViewport->GetWindow() : nullptr;
		}

		UGameUserSettings* GetSettings()
		{
			return GEngine ? GEngine->GetGameUserSettings() : nullptr;
		}
	}

	bool IsConfigurable()
	{
		return !GIsEditor && GetGameWindow().IsValid() && GetSettings() != nullptr;
	}

	void GetMonitors(TArray<FMonitorInfo>& OutMonitors)
	{
		FDisplayMetrics Metrics;
		FDisplayMetrics::RebuildDisplayMetrics(Metrics);
		OutMonitors = Metrics.MonitorInfo;
	}

	FString GetMonitorLabel(int32 MonitorIndex)
	{
		TArray<FMonitorInfo> Monitors;
		GetMonitors(Monitors);
		if (!Monitors.IsValidIndex(MonitorIndex))
		{
			return TEXT("Monitor ?");
		}
		const FMonitorInfo& Info = Monitors[MonitorIndex];
		return FString::Printf(TEXT("%d: %dx%d%s"), MonitorIndex + 1, Info.NativeWidth, Info.NativeHeight,
			Info.bIsPrimary ? TEXT(" (main)") : TEXT(""));
	}

	int32 GetPrimaryMonitorIndex()
	{
		TArray<FMonitorInfo> Monitors;
		GetMonitors(Monitors);
		for (int32 Index = 0; Index < Monitors.Num(); ++Index)
		{
			if (Monitors[Index].bIsPrimary)
			{
				return Index;
			}
		}
		return 0;
	}

	int32 GetCurrentMonitorIndex()
	{
		const TSharedPtr<SWindow> Window = GetGameWindow();
		if (!Window.IsValid())
		{
			return GetPrimaryMonitorIndex();
		}
		const FVector2D Position = FVector2D(Window->GetPositionInScreen());
		const FVector2D Size = FVector2D(Window->GetSizeInScreen());
		const FVector2D Center = Position + Size * 0.5;
		TArray<FMonitorInfo> Monitors;
		GetMonitors(Monitors);
		for (int32 Index = 0; Index < Monitors.Num(); ++Index)
		{
			const FPlatformRect& Rect = Monitors[Index].DisplayRect;
			if (Center.X >= Rect.Left && Center.X < Rect.Right && Center.Y >= Rect.Top && Center.Y < Rect.Bottom)
			{
				return Index;
			}
		}
		return GetPrimaryMonitorIndex();
	}

	EWindowMode::Type GetCurrentMode()
	{
		const UGameUserSettings* Settings = GetSettings();
		return Settings ? Settings->GetFullscreenMode() : EWindowMode::Windowed;
	}

	FIntPoint GetCurrentResolution()
	{
		const UGameUserSettings* Settings = GetSettings();
		return Settings ? Settings->GetScreenResolution() : FIntPoint(1280, 720);
	}

	void GetResolutions(EWindowMode::Type Mode, TArray<FIntPoint>& OutResolutions)
	{
		OutResolutions.Reset();
		if (Mode == EWindowMode::Windowed)
		{
			UKismetSystemLibrary::GetConvenientWindowedResolutions(OutResolutions);
		}
		else
		{
			UKismetSystemLibrary::GetSupportedFullscreenResolutions(OutResolutions);
		}
		OutResolutions.AddUnique(GetCurrentResolution());
		OutResolutions.Sort([](const FIntPoint& A, const FIntPoint& B)
		{
			return A.X * A.Y < B.X * B.Y || (A.X * A.Y == B.X * B.Y && A.X < B.X);
		});
	}

	void Apply(EWindowMode::Type Mode, FIntPoint Resolution, int32 MonitorIndex)
	{
		UGameUserSettings* Settings = GetSettings();
		const TSharedPtr<SWindow> Window = GetGameWindow();
		if (!Settings || !Window.IsValid() || GIsEditor)
		{
			return;
		}

		TArray<FMonitorInfo> Monitors;
		GetMonitors(Monitors);
		if (!Monitors.IsValidIndex(MonitorIndex))
		{
			MonitorIndex = GetPrimaryMonitorIndex();
		}
		if (!Monitors.IsValidIndex(MonitorIndex))
		{
			return;
		}
		const FMonitorInfo& Monitor = Monitors[MonitorIndex];
		const FPlatformRect& Work = Monitor.WorkArea;
		const FPlatformRect& Display = Monitor.DisplayRect;

		if (Mode == EWindowMode::WindowedFullscreen)
		{
			// Borderless always covers the whole monitor at its native size.
			Resolution = FIntPoint(Monitor.NativeWidth, Monitor.NativeHeight);
		}
		else if (Mode == EWindowMode::Windowed)
		{
			// Keep the window inside the monitor's usable area.
			Resolution.X = FMath::Min(Resolution.X, Work.Right - Work.Left);
			Resolution.Y = FMath::Min(Resolution.Y, Work.Bottom - Work.Top);
		}

		// Leave fullscreen first so the window can be moved to the chosen monitor.
		if (Window->GetWindowMode() != EWindowMode::Windowed)
		{
			Window->SetWindowMode(EWindowMode::Windowed);
		}
		FIntPoint Position(Display.Left, Display.Top);
		if (Mode == EWindowMode::Windowed)
		{
			Position.X = Work.Left + FMath::Max(0, ((Work.Right - Work.Left) - Resolution.X) / 2);
			Position.Y = Work.Top + FMath::Max(0, ((Work.Bottom - Work.Top) - Resolution.Y) / 2);
		}
		Window->MoveWindowTo(FVector2D(Position.X, Position.Y));

		Settings->SetFullscreenMode(Mode);
		Settings->SetScreenResolution(Resolution);
		Settings->SetWindowPosition(Position.X, Position.Y);
		Settings->ApplySettings(false); // applies the video mode and saves GameUserSettings.ini

		UE_LOG(LogEvolution, Log, TEXT("Display: %s %dx%d on monitor %d"), *GetModeLabel(Mode).ToString(), Resolution.X, Resolution.Y, MonitorIndex + 1);
	}

	FText GetModeLabel(EWindowMode::Type Mode)
	{
		switch (Mode)
		{
		case EWindowMode::Fullscreen:         return LOCTEXT("Fullscreen", "Fullscreen");
		case EWindowMode::WindowedFullscreen: return LOCTEXT("Borderless", "Borderless fullscreen");
		default:                              return LOCTEXT("Windowed", "Windowed");
		}
	}
}

#undef LOCTEXT_NAMESPACE
