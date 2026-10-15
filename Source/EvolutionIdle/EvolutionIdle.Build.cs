using UnrealBuildTool;

public class EvolutionIdle : ModuleRules
{
	public EvolutionIdle(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"Json",
			"JsonUtilities",
			"ApplicationCore"
		});

		// Steam achievements (Windows). The engine bundles the Steamworks SDK; SteamShared loads it.
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDependencyModuleNames.Add("SteamShared");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");
			PrivateDefinitions.Add("EVO_WITH_STEAM=1");
		}
		else
		{
			PrivateDefinitions.Add("EVO_WITH_STEAM=0");
		}
	}
}
