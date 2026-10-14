using UnrealBuildTool;
using System.Collections.Generic;

public class EvolutionIdleTarget : TargetRules
{
	public EvolutionIdleTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("EvolutionIdle");
	}
}
