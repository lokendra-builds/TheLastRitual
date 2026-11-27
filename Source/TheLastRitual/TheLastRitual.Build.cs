using UnrealBuildTool;

public class TheLastRitual : ModuleRules
{
	public TheLastRitual(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AssetRegistry",
			"NavigationSystem",
			"AudioExtensions",
			"Slate",
			"SlateCore"
		});
	}
}
