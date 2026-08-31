using UnrealBuildTool;

public class XSJEnvironmentToolsEditor : ModuleRules
{
	public XSJEnvironmentToolsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.Add("Core");
	}
}
