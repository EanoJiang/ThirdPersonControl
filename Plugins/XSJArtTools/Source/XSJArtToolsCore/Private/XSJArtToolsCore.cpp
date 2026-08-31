#include "XSJArtToolsCore.h"

#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "FXSJArtToolsCoreModule"

const FName FXSJArtToolsMenuNames::Root(TEXT("LevelEditor.MainMenu.XSJArtTools"));
const FName FXSJArtToolsMenuNames::AssetTools(TEXT("LevelEditor.MainMenu.XSJArtTools.AssetTools"));
const FName FXSJArtToolsMenuNames::CharacterTools(TEXT("LevelEditor.MainMenu.XSJArtTools.CharacterTools"));
const FName FXSJArtToolsMenuNames::EnvironmentTools(TEXT("LevelEditor.MainMenu.XSJArtTools.EnvironmentTools"));
const FName FXSJArtToolsMenuNames::BlueprintToJson(TEXT("LevelEditor.MainMenu.XSJArtTools.BlueprintToJson"));
const FName FXSJArtToolsMenuNames::ControlRigToJson(TEXT("LevelEditor.MainMenu.XSJArtTools.ControlRigToJson"));
const FName FXSJArtToolsMenuNames::SkeletonRefPoseToJson(TEXT("LevelEditor.MainMenu.XSJArtTools.SkeletonRefPoseToJson"));

void FXSJArtToolsCoreModule::StartupModule()
{
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FXSJArtToolsCoreModule::RegisterMenus)
	);
}

void FXSJArtToolsCoreModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
}

void FXSJArtToolsCoreModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* MainMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu"));
	if (MainMenu == nullptr)
	{
		return;
	}

	UToolMenu* RootMenu = MainMenu->AddSubMenu(
		this,
		TEXT("XSJArtTools"),
		TEXT("XSJArtTools"),
		LOCTEXT("XSJArtToolsMenuLabel", "XSJArtTools"),
		LOCTEXT("XSJArtToolsMenuTooltip", "XSJ art production tools")
	);

	if (RootMenu == nullptr)
	{
		return;
	}

	RootMenu->AddSubMenu(
		this,
		TEXT("XSJArtToolsCategories"),
		TEXT("AssetTools"),
		LOCTEXT("AssetToolsMenuLabel", "AssetTools"),
		LOCTEXT("AssetToolsMenuTooltip", "Asset tools")
	);

	RootMenu->AddSubMenu(
		this,
		TEXT("XSJArtToolsCategories"),
		TEXT("CharacterTools"),
		LOCTEXT("CharacterToolsMenuLabel", "CharacterTools"),
		LOCTEXT("CharacterToolsMenuTooltip", "Character tools")
	);

	RootMenu->AddSubMenu(
		this,
		TEXT("XSJArtToolsCategories"),
		TEXT("EnvironmentTools"),
		LOCTEXT("EnvironmentToolsMenuLabel", "EnvironmentTools"),
		LOCTEXT("EnvironmentToolsMenuTooltip", "Environment tools")
	);

	RootMenu->AddSubMenu(
		this,
		TEXT("XSJArtToolsCategories"),
		TEXT("BlueprintToJson"),
		LOCTEXT("BlueprintToJsonMenuLabel", "BlueprintToJson"),
		LOCTEXT("BlueprintToJsonMenuTooltip", "Blueprint to JSON tools")
	);

	RootMenu->AddSubMenu(
		this,
		TEXT("XSJArtToolsCategories"),
		TEXT("ControlRigToJson"),
		LOCTEXT("ControlRigToJsonMenuLabel", "ControlRigToJson"),
		LOCTEXT("ControlRigToJsonMenuTooltip", "Control Rig to JSON tools")
	);

	RootMenu->AddSubMenu(
		this,
		TEXT("XSJArtToolsCategories"),
		TEXT("SkeletonRefPoseToJson"),
		LOCTEXT("SkeletonRefPoseToJsonMenuLabel", "SkeletonRefPoseToJson"),
		LOCTEXT("SkeletonRefPoseToJsonMenuTooltip", "Skeleton reference-pose to JSON tools")
	);
}

IMPLEMENT_MODULE(FXSJArtToolsCoreModule, XSJArtToolsCore)

#undef LOCTEXT_NAMESPACE
