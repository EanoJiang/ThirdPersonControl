using UnrealBuildTool;

public class XSJControlRigToJsonEditor : ModuleRules
{
    public XSJControlRigToJsonEditor(ReadOnlyTargetRules Target) : base(Target)
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
                "BlueprintGraph",
                "ControlRig",
                "ControlRigDeveloper",
                "DesktopPlatform",
                "EditorFramework",
                "EditorStyle",
                "EditorSubsystem",
                "EditorWidgets",
                "GraphEditor",
                "InputCore",
                "Json",
                "JsonUtilities",
                "Kismet",
                "Projects",
                "RenderCore",
                "RigVM",
                "RigVMDeveloper",
                "Slate",
                "SlateCore",
                "ApplicationCore",
                "ToolMenus",
                "UnrealEd",
                "XSJArtToolsCore"
            }
        );
    }
}
