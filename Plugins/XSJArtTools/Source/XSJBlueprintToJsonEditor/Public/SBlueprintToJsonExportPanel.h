// 版权所有 Epic Games, Inc. 保留所有权利。

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

struct FAssetData;
class FActiveTimerHandle;
class SProgressBar;
class SEditableTextBox;
class STextBlock;
class SButton;

/**
 * 蓝图导出面板。
 * 选择源 Content 目录 + 输出目录，然后把源目录下的每个蓝图导出为独立的 JSON 文件。
 * 导出通过活动定时器（ActiveTimer）逐帧推进，使进度条能动画显示、窗口保持响应不卡死。
 */
class SBlueprintToJsonExportPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueprintToJsonExportPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** "浏览"按钮处理：选择源（输入）目录。 */
	FReply OnBrowseInputClicked();

	/** "浏览"按钮处理：选择输出目录。 */
	FReply OnBrowseOutputClicked();

	/** "导出"按钮处理：校验输入并启动逐帧导出。 */
	FReply OnExportClicked();

	/** 每帧步进：导出一个待处理蓝图并更新进度。队列清空时停止。 */
	EActiveTimerReturnType StepExport(double CurrentTime, float DeltaSeconds);

	/** 进度条的比例 [0..1]。 */
	TOptional<float> GetProgressFraction() const;

	/** 导出按钮是否可用（导出进行中禁用）。 */
	bool IsExportEnabled() const { return !bIsExporting; }

	// ---- UI 控件 ----
	TSharedPtr<SEditableTextBox> InputTextBox;   // 源目录输入框
	TSharedPtr<SEditableTextBox> OutputTextBox;  // 输出目录输入框
	TSharedPtr<SProgressBar> ProgressBar;        // 进度条
	TSharedPtr<STextBlock> StatusText;           // 状态文本
	TSharedPtr<SButton> ExportButton;            // 导出按钮

	// ---- 导出状态 ----
	bool bIsExporting = false;                   // 是否正在导出
	int32 ExportedCount = 0;                     // 已导出数量
	int32 TotalCount = 0;                        // 待导出总数
	TArray<FAssetData> PendingAssets;            // 待导出资产队列
	TArray<TSharedPtr<FJsonObject>> IndexEntries;// 已导出蓝图的索引条目（供最终写 index.json）
	FString CurrentInputGamePath;                // 当前导出对应的 /Game/... 路径
	FString CurrentOutputDir;                    // 当前导出的输出目录
	TWeakPtr<FActiveTimerHandle> TimerHandle;    // 活动定时器句柄（弱引用）
};
