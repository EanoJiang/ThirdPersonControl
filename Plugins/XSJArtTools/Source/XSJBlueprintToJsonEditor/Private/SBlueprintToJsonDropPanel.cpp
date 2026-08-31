// 版权所有 Epic Games, Inc. 保留所有权利。

#include "SBlueprintToJsonDropPanel.h"

#include "BlueprintToJsonExporter.h"

#include "AssetRegistry/AssetData.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "SDropTarget.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateColor.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SBlueprintToJsonDropPanel"

FString SBlueprintToJsonDropPanel::AssetKey(const FAssetData& InAsset)
{
	return InAsset.PackageName.ToString() + TEXT(".") + InAsset.AssetName.ToString();
}

void SBlueprintToJsonDropPanel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		// 整个窗口都是一个放置区：可把资产拖到面板任意位置。
		SNew(SDropTarget)
		.OnAllowDrop(this, &SBlueprintToJsonDropPanel::HandleAllowDrop)
		.OnDropped(this, &SBlueprintToJsonDropPanel::HandleDropped)
		[
			SNew(SVerticalBox)

			// 放置提示（始终可见；放置区即整个窗口）。
			+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 8.0f, 8.0f, 4.0f)
			[
				SNew(SBorder)
				.Padding(16.0f)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().HAlign(HAlign_Center).AutoHeight()
					[
						SNew(STextBlock).Text(LOCTEXT("DropHint", "拖拽蓝图资产到本面板任意处"))
					]
					+ SVerticalBox::Slot().HAlign(HAlign_Center).AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("DropHintSub", "UBlueprint（含 Anim/Widget 等蓝图子类）"))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
				]
			]

			// 资产列表。
			+ SVerticalBox::Slot().FillHeight(1.0f).Padding(8.0f, 0.0f, 8.0f, 4.0f)
			[
				SAssignNew(ListView, SListView<TSharedPtr<FAssetData>>)
				.ListItemsSource(&DraggedAssets)
				.OnGenerateRow(this, &SBlueprintToJsonDropPanel::MakeAssetRowWidget)
				.SelectionMode(ESelectionMode::Single)
			]

			// 清空全部按钮。
			+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 0.0f, 8.0f, 4.0f).HAlign(HAlign_Left)
			[
				SNew(SButton)
				.Text(LOCTEXT("ClearButton", "清空全部"))
				.OnClicked(this, &SBlueprintToJsonDropPanel::OnClearClicked)
				.IsEnabled_Lambda([this]() { return !bIsExporting && DraggedAssets.Num() > 0; })
			]

			// 导出按钮。
			+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 4.0f, 8.0f, 4.0f)
			[
				SAssignNew(ExportButton, SButton)
				.Text(LOCTEXT("ExportButton", "导出"))
				.HAlign(HAlign_Center)
				.OnClicked(this, &SBlueprintToJsonDropPanel::OnExportClicked)
				.IsEnabled(this, &SBlueprintToJsonDropPanel::IsExportEnabled)
			]

			// 进度条。
			+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 4.0f, 8.0f, 2.0f)
			[
				SNew(SProgressBar)
				.Percent(this, &SBlueprintToJsonDropPanel::GetProgressFraction)
			]

			// 状态文本。
			+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 0.0f, 8.0f, 8.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("StatusIdle", "就绪"))
			]
		]
	];
}

bool SBlueprintToJsonDropPanel::HandleAllowDrop(TSharedPtr<FDragDropOperation> DragOperation) const
{
	if (!DragOperation.IsValid())
	{
		return false;
	}

	const TArray<FAssetData> Dragged = AssetUtil::ExtractAssetDataFromDrag(DragOperation);
	for (const FAssetData& Asset : Dragged)
	{
		if (FBlueprintToJsonExporter::IsAssetClass(Asset))
		{
			return true;
		}
	}
	return false;
}

FReply SBlueprintToJsonDropPanel::HandleDropped(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	(void)MyGeometry;
	if (bIsExporting)
	{
		return FReply::Handled();
	}

	const TArray<FAssetData> Dragged = AssetUtil::ExtractAssetDataFromDrag(DragDropEvent);

	// 对已接受的资产去重。
	TSet<FString> ExistingKeys;
	for (const TSharedPtr<FAssetData>& Existing : DraggedAssets)
	{
		if (Existing.IsValid())
		{
			ExistingKeys.Add(AssetKey(*Existing));
		}
	}

	int32 Added = 0;
	for (const FAssetData& Asset : Dragged)
	{
		if (!FBlueprintToJsonExporter::IsAssetClass(Asset))
		{
			continue;
		}
		const FString Key = AssetKey(Asset);
		if (ExistingKeys.Contains(Key))
		{
			continue;
		}
		ExistingKeys.Add(Key);
		DraggedAssets.Add(MakeShared<FAssetData>(Asset));
		++Added;
	}

	if (ListView.IsValid())
	{
		ListView->RebuildList();
	}

	if (StatusText.IsValid())
	{
		StatusText->SetText(FText::Format(
			LOCTEXT("StatusAdded", "已添加 {0} 个，列表共 {1} 个资产"), Added, DraggedAssets.Num()));
	}

	return FReply::Handled();
}

TSharedRef<ITableRow> SBlueprintToJsonDropPanel::MakeAssetRowWidget(TSharedPtr<FAssetData> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FAssetData>>, OwnerTable)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 2.0f, 6.0f, 2.0f)
			[
				SNew(SImage).Image(FAppStyle::GetBrush("ClassIcon.Blueprint"))
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([Item]() { return Item.IsValid() ? FText::FromString(Item->AssetName.ToString()) : FText::GetEmpty(); })
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([Item]() { return Item.IsValid() ? FText::FromString(Item->AssetClassPath.GetAssetName().ToString()) : FText::GetEmpty(); })
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
			// 行内移除按钮。
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("RemoveRow", "x"))
				.ButtonStyle(FAppStyle::Get(), "NoBorder")
				.ContentPadding(FMargin(2.0f, 0.0f))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.IsEnabled_Lambda([this]() { return !bIsExporting; })
				.OnClicked(this, &SBlueprintToJsonDropPanel::OnRemoveAssetClicked, Item)
			]
		];
}

FReply SBlueprintToJsonDropPanel::OnRemoveAssetClicked(TSharedPtr<FAssetData> Item)
{
	if (bIsExporting || !Item.IsValid())
	{
		return FReply::Handled();
	}

	// 用稳定键（Package.Asset）匹配，而非指针身份。
	const FString Key = AssetKey(*Item);
	for (int32 i = 0; i < DraggedAssets.Num(); ++i)
	{
		if (DraggedAssets[i].IsValid() && AssetKey(*DraggedAssets[i]) == Key)
		{
			DraggedAssets.RemoveAt(i);
			break;
		}
	}

	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}
	if (StatusText.IsValid())
	{
		StatusText->SetText(FText::Format(
			LOCTEXT("StatusRemoved", "已移除，列表剩余 {0} 个资产"), DraggedAssets.Num()));
	}
	return FReply::Handled();
}

FReply SBlueprintToJsonDropPanel::OnClearClicked()
{
	if (bIsExporting)
	{
		return FReply::Handled();
	}

	DraggedAssets.Reset();
	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}
	if (StatusText.IsValid())
	{
		StatusText->SetText(LOCTEXT("StatusCleared", "已清空"));
	}
	return FReply::Handled();
}

FReply SBlueprintToJsonDropPanel::OnExportClicked()
{
	if (bIsExporting || DraggedAssets.Num() == 0)
	{
		return FReply::Handled();
	}

	// 在点击导出时才选择输出目录。
	FString OutputDir;
	const FString DefaultPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	if (!FBlueprintToJsonExporter::PickFolder(LOCTEXT("PickOutputFolder", "选择导出目录"), DefaultPath, OutputDir))
	{
		return FReply::Handled();
	}

	// 把已接受的资产快照进逐帧导出队列。
	PendingAssets.Reset();
	for (const TSharedPtr<FAssetData>& Asset : DraggedAssets)
	{
		if (Asset.IsValid())
		{
			PendingAssets.Add(*Asset);
		}
	}

	TotalCount = PendingAssets.Num();
	ExportedCount = 0;
	IndexEntries.Reset();
	CurrentOutputDir = OutputDir;
	bIsExporting = true;

	if (StatusText.IsValid())
	{
		StatusText->SetText(FText::Format(LOCTEXT("StatusStarting", "导出中 0/{0}"), TotalCount));
	}

	RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SBlueprintToJsonDropPanel::StepExport));
	return FReply::Handled();
}

EActiveTimerReturnType SBlueprintToJsonDropPanel::StepExport(double CurrentTime, float DeltaSeconds)
{
	if (!bIsExporting || PendingAssets.Num() == 0)
	{
		bIsExporting = false;
		return EActiveTimerReturnType::Stop;
	}

	// 本帧导出一个蓝图。
	const FAssetData Asset = PendingAssets.Pop(EAllowShrinking::No);

	TSharedPtr<FJsonObject> Entry;
	FString FileName;
	if (FBlueprintToJsonExporter::Get().ExportSingleAsset(Asset, CurrentOutputDir, Entry, FileName) && Entry.IsValid())
	{
		IndexEntries.Add(Entry);
		++ExportedCount;
	}

	if (StatusText.IsValid())
	{
		StatusText->SetText(FText::Format(LOCTEXT("StatusExporting", "导出中 {0}/{1}"), ExportedCount, TotalCount));
	}

	// 队列清空：收尾并停止定时器。
	if (PendingAssets.Num() == 0)
	{
		FBlueprintToJsonExporter::Get().WriteIndexJson(IndexEntries, FString(), CurrentOutputDir);
		bIsExporting = false;

		if (StatusText.IsValid())
		{
			StatusText->SetText(FText::Format(LOCTEXT("StatusExportDone", "完成，共导出 {0} 个蓝图"), ExportedCount));
		}
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("ExportDoneMessage", "导出完成，共 {0} 个蓝图。"), ExportedCount));
		return EActiveTimerReturnType::Stop;
	}

	return EActiveTimerReturnType::Continue;
}

TOptional<float> SBlueprintToJsonDropPanel::GetProgressFraction() const
{
	if (TotalCount <= 0)
	{
		return 0.0f;
	}
	return static_cast<float>(ExportedCount) / static_cast<float>(TotalCount);
}

#undef LOCTEXT_NAMESPACE