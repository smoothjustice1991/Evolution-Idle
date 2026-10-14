using UnrealBuildTool;
using System.Collections.Generic;

public class EvolutionIdleEditorTarget : TargetRules
{
	public EvolutionIdleEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("EvolutionIdle");
	}
}
