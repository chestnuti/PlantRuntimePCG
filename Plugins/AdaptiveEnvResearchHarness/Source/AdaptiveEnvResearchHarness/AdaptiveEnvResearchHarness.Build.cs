using UnrealBuildTool;
using System.IO;

public class AdaptiveEnvResearchHarness : ModuleRules
{
    public AdaptiveEnvResearchHarness(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "AdaptiveEnvRuntime",
            "PlantRuntimePCG"
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Json",
            "JsonUtilities"
        });

        // The game module keeps this exported actor header at its module root.
        PrivateIncludePaths.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "../../../../Source/PlantRuntimePCG")));
    }
}
