// 版权所有 Epic Games, Inc. 保留所有权利。

#pragma once

#include "CoreMinimal.h"

struct FAssetData;
class UEdGraph;
class UBlueprint;
class UObject;
struct FPropertyChangedEvent;
class UToolMenu;

/**
 * 蓝图转 JSON 导出器（单例）。
 * 注册一个编辑器菜单入口，把指定 Content 目录下的蓝图批量导出为
 * LLM 可读的 JSON 文件（每个蓝图一个文件）。
 */
class FBlueprintToJsonExporter
{
public:
	/** 获取单例实例。 */
	static FBlueprintToJsonExporter& Get();

	/** 注册菜单入口。在 GEditor 尚未创建时调用也安全。 */
	void Register();

	/** 注销菜单入口及待处理的启动回调。 */
	void Unregister();

	// ---- 以下为公开辅助函数，供导出面板及其他调用方使用 ----

	/** 递归收集指定 /Game/... 路径下的所有蓝图资产。资产注册表仍在加载时返回 false。 */
	bool GatherBlueprintAssets(const FString& InGamePath, TArray<FAssetData>& OutAssets);

	/** 导出单个蓝图资产到磁盘。成功时 OutEntry 被赋值为索引条目。返回是否成功。 */
	bool ExportSingleAsset(const FAssetData& InAsset, const FString& InOutputDir, TSharedPtr<FJsonObject>& OutEntry, FString& OutFileName);

	/** 将已收集的索引条目汇总写入 index.json。 */
	void WriteIndexJson(const TArray<TSharedPtr<FJsonObject>>& InEntries, const FString& InGamePath, const FString& InOutputDir);

	/** 把绝对路径的 Content 子目录映射回 /Game/... 包路径。静态方法，无需实例。 */
	static bool ContentFolderToGamePath(const FString& AbsoluteContentFolder, FString& OutGamePath, FString& OutError);

	/** 弹出文件夹选择对话框，用户取消时返回 false。静态方法，无需实例。 */
	static bool PickFolder(const FText& InTitle, const FString& InDefaultPath, FString& OutPath);

	/** 判断资产是否（或属于）蓝图基类/子类，用于拖放与右键过滤。 */
	static bool IsAssetClass(const FAssetData& InAsset);

private:
	/** ToolMenus 启动回调：编辑器就绪后真正挂载菜单项。 */
	void RegisterMenu();

	/** 菜单点击处理：弹出导出面板窗口。 */
	void OnExportClicked();

	/** 打开拖入导出面板窗口。 */
	void OpenDropPanel();

	/** 资产右键菜单处理：导出当前选中的单个蓝图。 */
	void OnExportSelectedClicked();

	/** 同步批量导出：发现 InGamePath 下的蓝图并逐个写出 JSON。返回导出数量，硬错误返回 -1。 */
	int32 ExportDirectory(const FString& InGamePath, const FString& InOutputDir);

	/** 构建描述单个蓝图的 JSON 对象。 */
	TSharedPtr<class FJsonObject> SerializeBlueprint(UBlueprint* InBlueprint);

	/** 将单个图（节点 + 连线）序列化为 JSON 对象。 */
	TSharedPtr<FJsonObject> SerializeGraph(UEdGraph* InGraph);

	/** 将单个蓝图的 JSON 写入磁盘，返回所用文件名。 */
	bool WriteBlueprintJson(const TSharedRef<class FJsonObject>& Object, const FString& AssetPackagePath, const FString& OutputDirectory, FString& OutFileName);

	/** 显示一个短暂的编辑器通知。 */
	void Notify(const FText& InMessage, bool bIsWarning);

	bool bRegisteredMenu = false;
};
