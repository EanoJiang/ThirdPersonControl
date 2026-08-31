#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

struct XSJARTTOOLSCORE_API FXSJArtToolsMenuNames
{
	static const FName Root;
	static const FName AssetTools;
	static const FName CharacterTools;
	static const FName EnvironmentTools;
	static const FName BlueprintToJson;
	static const FName ControlRigToJson;
	static const FName SkeletonRefPoseToJson;
};

class XSJARTTOOLSCORE_API FXSJArtToolsCoreModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
};
