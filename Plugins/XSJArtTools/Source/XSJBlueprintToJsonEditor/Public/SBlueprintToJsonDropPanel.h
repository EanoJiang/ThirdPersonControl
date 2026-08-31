// 版权所有 Epic Games, Inc. 保留所有权利。

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

struct FAssetData;
class FActiveTimerHandle;
class FDragDropEvent;
struct FGeometry;
class FDragDropOperation;
class SProgressBar;
class STextBlock;
class SButton;
class ITableRow;
class STableViewBase;
class FJsonObject;

template <typename ItemType> class SListView;

/**
 * 蓝图拖入导出面板（Slate 控件）。
 *
 * 从内容浏览器把任意蓝图（UBlueprint 及其子类）拖入本面板的放置区，再点击
 * "导出" 选择输出目录，即可把每个拖入的蓝图导出为独立的 JSON 文件
 * （外加 index.json 汇总）。与其它 ToJson 插件的拖入逻辑保持一致。
 *
 * 与旧的面板（按 Content 文件夹批量导出）不同：本面板以单个拖入资产为输入，
 * 输出目录在点击导出时才选择。导出通过活动定时器（ActiveTimer）逐帧推进，
 * 使进度条能动画显示、窗口保持响应。
 */
class SBlueprintToJsonDropPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueprintToJsonDropPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** SDropTarget：仅当拖动内容至少包含一个蓝图资产时允许放置。 */
	bool HandleAllowDrop(TSharedPtr<FDragDropOperation> DragOperation) const;

	/** SDropTarget：发生放置——按蓝图类型过滤、去重后追加到列表。 */
	FReply HandleDropped(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent);

	/** 生成资产列表中的一行。 */
	TSharedRef<ITableRow> MakeAssetRowWidget(TSharedPtr<FAssetData> Item, const TSharedRef<STableViewBase>& OwnerTable);

	/** 移除单个资产行（行内 x 按钮）。 */
	FReply OnRemoveAssetClicked(TSharedPtr<FAssetData> Item);

	FReply OnClearClicked();
	FReply OnExportClicked();

	/** 每帧导出队列中的一个资产，队列清空时停止。 */
	EActiveTimerReturnType StepExport(double CurrentTime, float DeltaSeconds);

	TOptional<float> GetProgressFraction() const;
	bool IsExportEnabled() const { return !bIsExporting && DraggedAssets.Num() > 0; }

	static FString AssetKey(const FAssetData& InAsset);

	// ---- UI 控件 ----
	TSharedPtr<SListView<TSharedPtr<FAssetData>>> ListView;
	TSharedPtr<SProgressBar> ProgressBar;
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<SButton> ExportButton;

	// ---- 已接受的源资产（用共享指针以被 SListView 持有） ----
	TArray<TSharedPtr<FAssetData>> DraggedAssets;

	// ---- 导出状态 ----
	bool bIsExporting = false;
	int32 ExportedCount = 0;
	int32 TotalCount = 0;
	TArray<FAssetData> PendingAssets;
	TArray<TSharedPtr<FJsonObject>> IndexEntries;
	FString CurrentOutputDir;
	TWeakPtr<FActiveTimerHandle> TimerHandle;
};