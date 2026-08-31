#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

struct FAssetData;
class FDragDropEvent;
struct FGeometry;
class FDragDropOperation;
class SProgressBar;
class STextBlock;
class SButton;
class ITableRow;
class STableViewBase;

template <typename ItemType> class SListView;

/**
 * Skeleton reference-pose drag-and-drop export panel (Slate widget).
 *
 * Drag one or more Skeleton / SkeletalMesh assets from the Content Browser onto
 * the drop zone, then click "Export" to pick an output directory and export each
 * dragged asset's reference pose to its own JSON file. Export is synchronous
 * (skeleton reference-pose extraction is lightweight).
 */
class SSkeletonRefPoseToJsonDropPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SSkeletonRefPoseToJsonDropPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    /** SDropTarget: allow the drop only if the drag carries at least one Skeleton/SkeletalMesh. */
    bool HandleAllowDrop(TSharedPtr<FDragDropOperation> DragOperation) const;

    /** SDropTarget: a drop happened - filter by Skeleton/SkeletalMesh, append (de-duplicated). */
    FReply HandleDropped(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent);

    /** Generate one row in the asset list view. */
    TSharedRef<ITableRow> MakeAssetRowWidget(TSharedPtr<FAssetData> Item, const TSharedRef<STableViewBase>& OwnerTable);

    /** Remove a single asset row (the per-row × button). */
    FReply OnRemoveAssetClicked(TSharedPtr<FAssetData> Item);

    FReply OnClearClicked();
    FReply OnExportClicked();

    TOptional<float> GetProgressFraction() const;
    bool IsExportEnabled() const { return !bIsExporting && DraggedAssets.Num() > 0; }

    static FString AssetKey(const FAssetData& InAsset);

    // ---- UI controls ----
    TSharedPtr<SListView<TSharedPtr<FAssetData>>> ListView;
    TSharedPtr<SProgressBar> ProgressBar;
    TSharedPtr<STextBlock> StatusText;
    TSharedPtr<SButton> ExportButton;

    // ---- accepted source assets ----
    TArray<TSharedPtr<FAssetData>> DraggedAssets;

    // ---- export state ----
    bool bIsExporting = false;
    float ExportProgress = 0.0f;
};
