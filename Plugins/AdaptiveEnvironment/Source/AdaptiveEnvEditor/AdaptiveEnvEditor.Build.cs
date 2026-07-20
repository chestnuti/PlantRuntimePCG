using UnrealBuildTool;

public class AdaptiveEnvEditor : ModuleRules
{
	public AdaptiveEnvEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Json",
			"UnrealEd",
			"AdaptiveEnvRuntime"
		});
	}
}
