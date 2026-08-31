#pragma once

#include "Modules/ModuleManager.h"

/**
 * ControlRigToJson plugin module.
 * Registers the Control Rig exporter (menu hook + popup export panel) on editor startup,
 * and unregisters on shutdown. The module itself holds no logic; it only serves as the
 * lifecycle host for the exporter.
 */
class FControlRigToJsonModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
