using UnrealBuildTool;

public class AdaptiveEnvTests : ModuleRules
{
	public AdaptiveEnvTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"AdaptiveEnvRuntime"
		});
	}
}
