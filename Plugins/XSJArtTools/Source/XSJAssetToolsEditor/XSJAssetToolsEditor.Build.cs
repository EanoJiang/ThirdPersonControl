using UnrealBuildTool;

public class XSJAssetToolsEditor : ModuleRules
{
	public XSJAssetToolsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"AssetRegistry",
				"EditorWidgets",
				"InputCore",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"UnrealEd",
				"XSJArtToolsCore"
			}
		);
	}
}
