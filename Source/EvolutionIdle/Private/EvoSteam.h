#pragma once

#include "CoreMinimal.h"

class FSteamClientInstanceHandler;

/**
 * Thin bridge to Steam achievements through the Steamworks SDK bundled with the engine.
 *
 * Achievements are tracked by the game itself (in the save file); this class only mirrors unlocks to Steam
 * when Steam is available. Everything is a no-op when Steam is off, not running, or this is the editor.
 * Settings live in DefaultEngine.ini under [EvolutionIdle.Steam].
 */
class FEvoSteam
{
public:
	~FEvoSteam();

	/** Starts the Steam API if enabled. Returns true when Steam achievements are available. */
	bool Init();
	void Shutdown();

	bool IsActive() const { return bActive; }

	/** Unlocks an achievement on Steam (no-op if Steam is off or it is already unlocked). */
	void Unlock(const FString& ApiName);

	/** True if Steam reports this achievement as already unlocked (e.g. earned on another PC). */
	bool IsUnlockedOnSteam(const FString& ApiName) const;

	/** Pumps Steam callbacks and uploads pending unlocks. Call every frame. */
	void Tick(float DeltaTime);

	/** Uploads pending unlocks immediately. */
	void Flush();

private:
	bool bActive = false;
	bool bStorePending = false;
	float StoreCooldown = 0.f;
	TSharedPtr<FSteamClientInstanceHandler> Client;
};
