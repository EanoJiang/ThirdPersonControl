// Fill out your copyright notice in the Description page of Project Settings.

#include "GraphShotCommands.h"

#include "Framework/Commands/InputChord.h"
#include "InputCoreTypes.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "GraphShotCommands"

FGraphShotCommands::FGraphShotCommands()
	: TCommands<FGraphShotCommands>(
		TEXT("GraphShot"),
		NSLOCTEXT("GraphShot", "GraphShotCommandsDisplayName", "GraphShot"),
		TEXT("MainFrame"),
		FAppStyle::GetAppStyleSetName())
{
}

void FGraphShotCommands::RegisterCommands()
{
	// Default chord: Ctrl + Alt + Shift + S (user-configurable in Editor Preferences > Keyboard Shortcuts > GraphShot).
	UI_COMMAND(
		CaptureGraph,
		"GraphShot",
		"Capture a complete screenshot of the current graph (all nodes, including off-screen) to the clipboard.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::S, /*bShift*/ true, /*bCtrl*/ true, /*bAlt*/ true, /*bCmd*/ false));
}

#undef LOCTEXT_NAMESPACE
