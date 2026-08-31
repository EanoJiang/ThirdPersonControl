using UnrealBuildTool;

public class XSJCharacterToolsEditor : ModuleRules
{
	public XSJCharacterToolsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.Add("Core");
	}
}
