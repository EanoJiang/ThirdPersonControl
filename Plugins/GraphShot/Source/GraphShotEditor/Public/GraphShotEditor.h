// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class IGraphShotEditorModule : public IModuleInterface
{
public:
	/** Singleton-like access to this module's interface. */
	static inline IGraphShotEditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IGraphShotEditorModule>("GraphShotEditor");
	}
};
