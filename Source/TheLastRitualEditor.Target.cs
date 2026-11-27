using UnrealBuildTool;
using System.Collections.Generic;

public class TheLastRitualEditorTarget : TargetRules
{
	public TheLastRitualEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("TheLastRitual");
	}
}
