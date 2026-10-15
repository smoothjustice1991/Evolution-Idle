#include "EvoSteam.h"
#include "EvolutionIdle.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreMisc.h"
#include "Modules/ModuleManager.h"
#include "HAL/PlatformMisc.h"

#ifndef EVO_WITH_STEAM
#define EVO_WITH_STEAM 0
#endif

#if EVO_WITH_STEAM
#include "SteamSharedModule.h"
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END
#endif

namespace
{
	const TCHAR* SteamSection = TEXT("EvolutionIdle.Steam");

	/** Seconds between uploads, so a burst of unlocks is sent in one StoreStats call. */
	constexpr float StoreInterval = 1.0f;
}

FEvoSteam::~FEvoSteam()
{
	Shutdown();
}

bool FEvoSteam::Init()
{
#if EVO_WITH_STEAM
	if (bActive)
	{
		return true;
	}

	bool bEnabled = true;
	GConfig->GetBool(SteamSection, TEXT("bEnabled"), bEnabled, GEngineIni);
	if (!bEnabled || IsRunningCommandlet())
	{
		UE_LOG(LogEvolution, Log, TEXT("Steam achievements disabled in config."));
		return false;
	}

	if (GIsEditor)
	{
		bool bEnableInEditor = false;
		GConfig->GetBool(SteamSection, TEXT("bEnableInEditor"), bEnableInEditor, GEngineIni);
		if (!bEnableInEditor)
		{
			UE_LOG(LogEvolution, Log, TEXT("Steam achievements are off in the editor; unlocks are saved locally."));
			return false;
		}
	}

	FSteamSharedModule* Module = FModuleManager::LoadModulePtr<FSteamSharedModule>(TEXT("SteamShared"));
	if (!Module || !Module->AreSteamDllsLoaded())
	{
		UE_LOG(LogEvolution, Warning, TEXT("Steam library not loaded; achievements are saved locally only."));
		return false;
	}

#if UE_BUILD_SHIPPING
	// In a release build started outside Steam, relaunch through Steam so achievements work.
	int32 ReleaseAppId = 0;
	GConfig->GetInt(SteamSection, TEXT("ReleaseAppId"), ReleaseAppId, GEngineIni);
	if (ReleaseAppId > 0 && SteamAPI_RestartAppIfNecessary(static_cast<uint32>(ReleaseAppId)))
	{
		UE_LOG(LogEvolution, Log, TEXT("Not started by Steam; relaunching through Steam."));
		FPlatformMisc::RequestExit(false, TEXT("EvoSteam.RestartThroughSteam"));
		return false;
	}
#endif

	Client = Module->ObtainSteamClientInstanceHandle();
	if (!Client.IsValid() || SteamUserStats() == nullptr)
	{
		Client.Reset();
		UE_LOG(LogEvolution, Log, TEXT("Steam is not running; achievements are saved locally and sync next time."));
		return false;
	}

	bActive = true;
	UE_LOG(LogEvolution, Log, TEXT("Steam achievements active (app %u)."), SteamUtils() ? SteamUtils()->GetAppID() : 0u);
	return true;
#else
	return false;
#endif
}

void FEvoSteam::Shutdown()
{
	if (bActive)
	{
		Flush();
	}
	bActive = false;
	Client.Reset();
}

void FEvoSteam::Unlock(const FString& ApiName)
{
#if EVO_WITH_STEAM
	if (!bActive || ApiName.IsEmpty())
	{
		return;
	}
	ISteamUserStats* Stats = SteamUserStats();
	if (!Stats)
	{
		return;
	}

	const FTCHARToUTF8 Name(*ApiName);
	bool bAlreadyUnlocked = false;
	if (Stats->GetAchievement(Name.Get(), &bAlreadyUnlocked) && bAlreadyUnlocked)
	{
		return;
	}
	if (Stats->SetAchievement(Name.Get()))
	{
		bStorePending = true;
	}
	else
	{
		UE_LOG(LogEvolution, Warning, TEXT("Steam rejected achievement '%s'. Is it defined and published in Steamworks for this App ID?"), *ApiName);
	}
#endif
}

bool FEvoSteam::IsUnlockedOnSteam(const FString& ApiName) const
{
#if EVO_WITH_STEAM
	if (!bActive || ApiName.IsEmpty())
	{
		return false;
	}
	ISteamUserStats* Stats = SteamUserStats();
	bool bUnlocked = false;
	return Stats && Stats->GetAchievement(FTCHARToUTF8(*ApiName).Get(), &bUnlocked) && bUnlocked;
#else
	return false;
#endif
}

void FEvoSteam::Tick(float DeltaTime)
{
#if EVO_WITH_STEAM
	if (!bActive)
	{
		return;
	}
	SteamAPI_RunCallbacks();

	StoreCooldown = FMath::Max(0.f, StoreCooldown - DeltaTime);
	if (bStorePending && StoreCooldown <= 0.f)
	{
		Flush();
	}
#endif
}

void FEvoSteam::Flush()
{
#if EVO_WITH_STEAM
	if (!bActive || !bStorePending)
	{
		return;
	}
	if (ISteamUserStats* Stats = SteamUserStats())
	{
		Stats->StoreStats();
	}
	bStorePending = false;
	StoreCooldown = StoreInterval;
#endif
}
