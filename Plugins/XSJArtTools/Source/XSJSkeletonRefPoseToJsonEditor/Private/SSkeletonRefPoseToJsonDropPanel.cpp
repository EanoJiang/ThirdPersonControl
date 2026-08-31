#include "SSkeletonRefPoseToJsonDropPanel.h"

#include "SkeletonRefPoseToJsonExporter.h"

#include "AssetRegistry/AssetData.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateColor.h"
#include "Textures/SlateIcon.h"
#include "SDropTarget.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SSkeletonRefPoseToJsonDropPanel"

FString SSkeletonRefPoseToJsonDropPanel::AssetKey(const FAssetData& InAsset)
{
    return InAsset.PackageName.ToString() + TEXT(".") + InAsset.AssetName.ToString();
}

void SSkeletonRefPoseToJsonDropPanel::Construct(const FArguments& InArgs)
{
    ChildSlot
    [
        // The whole window is a drop target: drag assets anywhere over the panel.
        SNew(SDropTarget)
        .OnAllowDrop(this, &SSkeletonRefPoseToJsonDropPanel::HandleAllowDrop)
        .OnDropped(this, &SSkeletonRefPoseToJsonDropPanel::HandleDropped)
        [
            SNew(SVerticalBox)

            // Drop hint (always visible; the drop zone is the entire window)
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
                        SNew(STextBlock).Text(LOCTEXT("DropHint", "拖拽 Skeleton / SkeletalMesh 资产到本面板任意处"))
                    ]
                    + SVerticalBox::Slot().HAlign(HAlign_Center).AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("DropHintSub", "USkeleton / USkeletalMesh"))
                        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                    ]
                ]
            ]

            // Asset list
            + SVerticalBox::Slot().FillHeight(1.0f).Padding(8.0f, 0.0f, 8.0f, 4.0f)
            [
                SAssignNew(ListView, SListView<TSharedPtr<FAssetData>>)
                .ListItemsSource(&DraggedAssets)
                .OnGenerateRow(this, &SSkeletonRefPoseToJsonDropPanel::MakeAssetRowWidget)
                .SelectionMode(ESelectionMode::Single)
            ]

            // Clear-all button
            + SVerticalBox::Slot().AutoHeight().Padding(8.0f, 0.0f, 8.0f, 4.0f).HAlign(HAlign_Left)
            [
                SNew(SButton)
                .Text(LOCTEXT("ClearButton", "清空全部"))
                .OnClicked(this, &SSkeletonRefPoseToJsonDropPanel::OnClearClicked)
                .IsEnabled_Lambda([this]() { return !bIsExporting && DraggedAssets.Num() > 0; })
            ]

            // Export button
            + SVerticalBox::Slot().AutoHeight().Padding(8.0f, 4.0f, 8.0f, 4.0f)
            [
                SAssignNew(ExportButton, SButton)
                .Text(LOCTEXT("ExportButton", "导出"))
                .HAlign(HAlign_Center)
                .OnClicked(this, &SSkeletonRefPoseToJsonDropPanel::OnExportClicked)
                .IsEnabled(this, &SSkeletonRefPoseToJsonDropPanel::IsExportEnabled)
            ]

            // Progress bar
            + SVerticalBox::Slot().AutoHeight().Padding(8.0f, 4.0f, 8.0f, 2.0f)
            [
                SAssignNew(ProgressBar, SProgressBar)
                .Percent(this, &SSkeletonRefPoseToJsonDropPanel::GetProgressFraction)
            ]

            // Status text
            + SVerticalBox::Slot().AutoHeight().Padding(8.0f, 0.0f, 8.0f, 8.0f)
            [
                SAssignNew(StatusText, STextBlock)
                .Text(LOCTEXT("StatusIdle", "就绪"))
            ]
        ]
    ];
}

bool SSkeletonRefPoseToJsonDropPanel::HandleAllowDrop(TSharedPtr<FDragDropOperation> DragOperation) const
{
    if (!DragOperation.IsValid())
    {
        return false;
    }

    const TArray<FAssetData> Dragged = AssetUtil::ExtractAssetDataFromDrag(DragOperation);
    for (const FAssetData& Asset : Dragged)
    {
        if (FSkeletonRefPoseToJsonExporter::IsSkeletonAssetClass(Asset))
        {
            return true;
        }
    }
    return false;
}

FReply SSkeletonRefPoseToJsonDropPanel::HandleDropped(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
    (void)MyGeometry;
    if (bIsExporting)
    {
        return FReply::Handled();
    }

    const TArray<FAssetData> Dragged = AssetUtil::ExtractAssetDataFromDrag(DragDropEvent);

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
        if (!FSkeletonRefPoseToJsonExporter::IsSkeletonAssetClass(Asset))
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
        ListView->RequestListRefresh();
    }

    if (StatusText.IsValid())
    {
        StatusText->SetText(FText::Format(
            LOCTEXT("StatusAdded", "已添加 {0} 个，列表共 {1} 个资产"), Added, DraggedAssets.Num()));
    }

    return FReply::Handled();
}

TSharedRef<ITableRow> SSkeletonRefPoseToJsonDropPanel::MakeAssetRowWidget(TSharedPtr<FAssetData> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
    return SNew(STableRow<TSharedPtr<FAssetData>>, OwnerTable)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 2.0f, 6.0f, 2.0f)
        [
            SNew(SImage).Image(FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Skeleton").GetIcon())
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
        // Per-row remove button
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
        [
            SNew(SButton)
            .Text(LOCTEXT("RemoveRow", "x")) // per-row remove
            .ButtonStyle(FAppStyle::Get(), "NoBorder")
            .ContentPadding(FMargin(2.0f, 0.0f))
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            .IsEnabled_Lambda([this]() { return !bIsExporting; })
            .OnClicked(this, &SSkeletonRefPoseToJsonDropPanel::OnRemoveAssetClicked, Item)
        ]
    ];
}

FReply SSkeletonRefPoseToJsonDropPanel::OnRemoveAssetClicked(TSharedPtr<FAssetData> Item)
{
    if (bIsExporting || !Item.IsValid())
    {
        return FReply::Handled();
    }

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

FReply SSkeletonRefPoseToJsonDropPanel::OnClearClicked()
{
    if (bIsExporting)
    {
        return FReply::Handled();
    }

    DraggedAssets.Reset();
    ExportProgress = 0.0f;
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

FReply SSkeletonRefPoseToJsonDropPanel::OnExportClicked()
{
    if (bIsExporting || DraggedAssets.Num() == 0)
    {
        return FReply::Handled();
    }

    // Pick the output directory at export time.
    FString OutputDir;
    const FString DefaultPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    if (!FSkeletonRefPoseToJsonExporter::PickFolder(LOCTEXT("PickOutputFolder", "选择导出目录"), DefaultPath, OutputDir))
    {
        return FReply::Handled();
    }

    bIsExporting = true;
    ExportProgress = 0.0f;
    if (StatusText.IsValid())
    {
        StatusText->SetText(LOCTEXT("StatusExporting", "导出中..."));
    }

    int32 OkCount = 0;
    int32 FailCount = 0;
    const int32 Total = DraggedAssets.Num();

    for (const TSharedPtr<FAssetData>& Asset : DraggedAssets)
    {
        if (!Asset.IsValid())
        {
            ++FailCount;
            continue;
        }

        UObject* TargetAsset = Asset->GetAsset();
        FString FileName;
        if (TargetAsset && FSkeletonRefPoseToJsonExporter::Get().ExportAsset(TargetAsset, OutputDir, FileName))
        {
            ++OkCount;
        }
        else
        {
            ++FailCount;
        }
    }

    ExportProgress = (Total > 0) ? 1.0f : 0.0f;
    bIsExporting = false;

    const FText DoneMessage = (FailCount == 0)
        ? FText::Format(LOCTEXT("StatusDone", "完成，已导出 {0} 个"), OkCount)
        : FText::Format(LOCTEXT("StatusDonePartial", "完成 {0} 个，失败 {1} 个"), OkCount, FailCount);

    if (StatusText.IsValid())
    {
        StatusText->SetText(DoneMessage);
    }
    FMessageDialog::Open(EAppMsgType::Ok, DoneMessage);
    return FReply::Handled();
}

TOptional<float> SSkeletonRefPoseToJsonDropPanel::GetProgressFraction() const
{
    return ExportProgress;
}

#undef LOCTEXT_NAMESPACE
