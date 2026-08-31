// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

/**
 * UI command + default hotkey for GraphShot.
 *
 * Parented to the "MainFrame" binding context (the global, lowest-priority context) so the
 * chord is active inside every editor (Blueprint / AnimBP / ControlRig / Material / ...),
 * not only the level editor.
 */
class FGraphShotCommands : public TCommands<FGraphShotCommands>
{
public:
	FGraphShotCommands();

	virtual void RegisterCommands() override;

	/** Capture a complete screenshot of the current graph to the clipboard. */
	TSharedPtr<FUICommandInfo> CaptureGraph;
};
