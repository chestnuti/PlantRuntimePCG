using UnrealBuildTool;

public class AdaptiveEnvRuntime : ModuleRules
{
	public AdaptiveEnvRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			"GameplayTags"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Json",
			"JsonUtilities",
			"NavigationSystem",
			"RenderCore",
			"RHI"
		});
	}
}
