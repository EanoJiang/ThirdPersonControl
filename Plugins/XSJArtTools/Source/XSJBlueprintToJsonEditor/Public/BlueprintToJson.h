// 版权所有 Epic Games, Inc. 保留所有权利。

#pragma once

#include "Modules/ModuleManager.h"

/**
 * BlueprintToJson 插件模块。
 * 在编辑器启动时注册蓝图导出器（挂载菜单 + 弹出导出面板），
 * 关闭时注销。模块本身不含逻辑，仅作为导出器的生命周期宿主。
 */
class FBlueprintToJsonModule : public IModuleInterface
{
public:
	/** 模块加载：注册导出器（菜单入口）。 */
	virtual void StartupModule() override;

	/** 模块卸载：注销导出器。 */
	virtual void ShutdownModule() override;
};
