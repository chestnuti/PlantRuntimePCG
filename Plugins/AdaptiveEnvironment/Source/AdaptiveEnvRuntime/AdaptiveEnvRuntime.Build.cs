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
			"GameplayTags",
			"GeometryFramework",
			"GeometryCore",
			"Niagara"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Json",
			"Landscape",
			"NavigationSystem",
			"RenderCore",
			"RHI"
		});
	}
}
