// 版权所有 Epic Games, Inc. 保留所有权利。

#include "BlueprintToJson.h"

#include "BlueprintToJsonExporter.h"

#define LOCTEXT_NAMESPACE "FBlueprintToJsonModule"

void FBlueprintToJsonModule::StartupModule()
{
	// 模块加载：注册蓝图导出器单例（挂载 BluePrintTool 菜单）。
	FBlueprintToJsonExporter::Get().Register();
}

void FBlueprintToJsonModule::ShutdownModule()
{
	// 模块卸载：注销导出器，清理菜单与回调。
	FBlueprintToJsonExporter::Get().Unregister();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBlueprintToJsonModule, XSJBlueprintToJsonEditor)
