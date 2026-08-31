#pragma once

#include "Modules/ModuleManager.h"

/**
 * SkeletonRefPoseToJson plugin module.
 * Registers the skeleton reference-pose exporter (Content Browser menu hook) on
 * editor startup and unregisters it on shutdown. The module itself holds no logic;
 * it only serves as the lifecycle host for FSkeletonRefPoseToJsonExporter.
 */
class FSkeletonRefPoseToJsonModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
