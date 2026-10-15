#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericWindowDefinition.h"

struct FMonitorInfo;

/** Window mode, resolution and monitor handling for the settings menu. */
namespace EvoDisplay
{
	/** False in the editor (PIE), where changing the window would move or resize the editor itself. */
	bool IsConfigurable();

	void GetMonitors(TArray<FMonitorInfo>& OutMonitors);
	FString GetMonitorLabel(int32 MonitorIndex);
	int32 GetPrimaryMonitorIndex();

	/** Index of the monitor that currently contains the centre of the game window. */
	int32 GetCurrentMonitorIndex();

	/** Resolutions offered for the given mode (largest last), always including the current one. */
	void GetResolutions(EWindowMode::Type Mode, TArray<FIntPoint>& OutResolutions);

	EWindowMode::Type GetCurrentMode();
	FIntPoint GetCurrentResolution();

	/** Applies and saves the window mode and resolution, moving the game window onto the given monitor. */
	void Apply(EWindowMode::Type Mode, FIntPoint Resolution, int32 MonitorIndex);

	FText GetModeLabel(EWindowMode::Type Mode);
}
