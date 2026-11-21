using UnrealBuildTool;

public class SurvivalWorldEditor : ModuleRules
{
    public SurvivalWorldEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "Landscape", "UnrealEd" });
    }
}
