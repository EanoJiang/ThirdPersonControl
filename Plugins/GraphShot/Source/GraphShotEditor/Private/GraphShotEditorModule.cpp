// Fill out your copyright notice in the Description page of Project Settings.

#include "GraphShotEditor.h"
#include "GraphShotCapture.h"
#include "GraphShotCommands.h"

#include "Framework/Commands/InputBindingManager.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "ToolMenuSection.h"
#include "ToolMenus.h"
#include "Toolkits/AssetEditorToolkit.h"

#define LOCTEXT_NAMESPACE "GraphShotEditor"

class FGraphShotEditorModule : public IGraphShotEditorModule
{
public:
	virtual void StartupModule() override
	{
		// 1) Register the command + default chord.
		FGraphShotCommands::Register();

		// 2) Map the action and expose the command list so the chord is active in every editor
		//    (the command's binding context is parented to "MainFrame", the global context).
		CommandList = MakeShared<FUICommandList>();
		CommandList->MapAction(
			FGraphShotCommands::Get().CaptureGraph,
			FExecuteAction::CreateLambda([]() { FGraphShotCapture::CaptureToClipboard(); }),
			FCanExecuteAction::CreateLambda([]() { return FGraphShotCapture::CanCapture(); }));

		if (FInputBindingManager::Get().RegisterCommandList(FGraphShotCommands::Get().GetContextName(), CommandList.ToSharedRef()) == false)
		{
			UE_LOG(LogTemp, Warning, TEXT("GraphShot: failed to register command list with the input binding manager."));
		}

		// 3) Toolbar button in EVERY asset editor (BP / AnimBP / ControlRig / Material / ...).
		RegisterToolbarExtender();

		// 4) Global menu entry (always visible in the main menu bar).
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FGraphShotEditorModule::RegisterMenus));
	}

	virtual void ShutdownModule() override
	{
		if (UToolMenus::Get() != nullptr)
		{
			UToolMenus::UnRegisterStartupCallback(this);
			UToolMenus::UnregisterOwner(this);
		}

		if (CommandList.IsValid())
		{
			FInputBindingManager::Get().UnregisterCommandList(FGraphShotCommands::Get().GetContextName(), CommandList.ToSharedRef());
		}

		// Note: FExtensibilityManager has no RemoveExtender; the shared extender lives for the editor
		// lifetime (standard for editor plugins). Drop our reference.
		ToolbarExtender.Reset();
		CommandList.Reset();

		FGraphShotCommands::Unregister();
	}

private:
	void RegisterToolbarExtender()
	{
		ToolbarExtender = MakeShared<FExtender>();

		ToolbarExtender->AddToolBarExtension(
			"Asset",
			EExtensionHook::After,
			CommandList,
			FToolBarExtensionDelegate::CreateLambda([](FToolBarBuilder& Builder)
			{
				Builder.AddToolBarButton(
					FUIAction(
						FExecuteAction::CreateLambda([]() { FGraphShotCapture::CaptureToClipboard(); }),
						FCanExecuteAction::CreateLambda([]() { return FGraphShotCapture::CanCapture(); })),
					NAME_None,
					LOCTEXT("ToolbarLabel", "GraphShot"),
					LOCTEXT("ToolbarTip", "Capture a complete screenshot of the current graph (all nodes, including off-screen) to the clipboard."),
					FSlateIcon());
			}));

		TSharedPtr<FExtensibilityManager> SharedToolBarManager = FAssetEditorToolkit::GetSharedToolBarExtensibilityManager();
		if (SharedToolBarManager.IsValid())
		{
			SharedToolBarManager->AddExtender(ToolbarExtender);
		}
	}

	void RegisterMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(this);

		if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window"))
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("GraphShot");
			Section.AddMenuEntry(
				"GraphShot.Capture",
				LOCTEXT("MenuLabel", "GraphShot: Screenshot Current Graph"),
				LOCTEXT("MenuTip", "Capture a complete screenshot of the current graph (all nodes, including off-screen) to the clipboard."),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([]() { FGraphShotCapture::CaptureToClipboard(); }),
					FCanExecuteAction::CreateLambda([]() { return FGraphShotCapture::CanCapture(); })));
		}
	}

	TSharedPtr<FUICommandList> CommandList;
	TSharedPtr<FExtender> ToolbarExtender;
};

IMPLEMENT_MODULE(FGraphShotEditorModule, GraphShotEditor)

#undef LOCTEXT_NAMESPACE
