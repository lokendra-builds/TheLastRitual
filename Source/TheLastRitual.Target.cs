using UnrealBuildTool;
using System.Collections.Generic;

public class TheLastRitualTarget : TargetRules
{
	public TheLastRitualTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("TheLastRitual");
	}
}
