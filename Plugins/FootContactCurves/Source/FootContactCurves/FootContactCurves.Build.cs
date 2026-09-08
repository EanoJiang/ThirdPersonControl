// Copyright (c) 2026 Shane Beal. Licensed under the MIT License.

using UnrealBuildTool;

public class FootContactCurves : ModuleRules
{
	public FootContactCurves(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AnimationModifiers"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AnimationBlueprintLibrary",
			"UnrealEd"
		});
	}
}
