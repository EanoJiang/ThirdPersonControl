using UnrealBuildTool;

public class GraphShotEditor : ModuleRules
{
	public GraphShotEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",        // EKeys, EModifierKey, FInputChord
				"ApplicationCore",  // FSlateApplication, Windows platform types
				"SlateCore",        // SWidget, FGeometry, FChildren, FAppStyle, SlatePrepass
				"Slate",            // TCommands, FUICommandInfo/List, FBindingContext, FInputBindingManager, FExtender, FToolBarBuilder, FMultiBox
				"AppFramework",     // toolbar/menu builder helpers
				"UnrealEd",         // FAssetEditorToolkit (shared toolbar extender), UToolMenus host, notifications
				"GraphEditor",      // SGraphPanel, SNodePanel, SGraphNode, FNodeFactory (temp panel + bounds)
				"RHI",              // GetMax2DTextureDimension, FReadSurfaceDataFlags, RCM_UNorm
				"RenderCore",       // FlushRenderingCommands, FRenderTarget plumbing
				"UMG",              // FWidgetRenderer, UTextureRenderTarget2D draw path
				"ImageWrapper",     // (reserved) PNG fallback path
				"ToolMenus",        // UToolMenus menu entry
				"EditorFramework",  // editor command plumbing
			}
		);
	}
}
