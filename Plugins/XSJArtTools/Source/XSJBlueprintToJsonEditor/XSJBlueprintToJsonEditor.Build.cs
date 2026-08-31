using UnrealBuildTool;

public class XSJBlueprintToJsonEditor : ModuleRules
{
	public XSJBlueprintToJsonEditor(ReadOnlyTargetRules Target) : base(Target)
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
