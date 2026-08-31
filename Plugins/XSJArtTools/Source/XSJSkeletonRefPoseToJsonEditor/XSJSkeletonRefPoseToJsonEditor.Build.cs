using UnrealBuildTool;

public class XSJSkeletonRefPoseToJsonEditor : ModuleRules
{
    public XSJSkeletonRefPoseToJsonEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "AssetRegistry",
                "AssetTools",
                "DesktopPlatform",
                "EditorFramework",
                "EditorStyle",
                "EditorSubsystem",
                "InputCore",
                "Json",
                "JsonUtilities",
                "Projects",
                "RenderCore",
                "Slate",
                "SlateCore",
                "ApplicationCore",
                "EditorWidgets",
                "ToolMenus",
                "UnrealEd",
                "XSJArtToolsCore",
            }
        );
    }
}
