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

template <typename ItemType> class SListView;

/**
 * Control Rig drag-and-drop export panel (Slate widget).
 *
 * Drag one or more Control Rig assets (UControlRigBlueprint / UControlRigRuntimeAsset)
 * from the Content Browser onto the drop zone, then click "Export" to pick an output
 * directory and export each dragged rig to its own JSON file (plus an index.json).
 *
 * The export is driven by an ActiveTimer so the progress bar updates and the window
 * stays responsive, mirroring SControlRigToJsonExportPanel's per-frame export.
 */
class SControlRigToJsonDropPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SControlRigToJsonDropPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    /** SDropTarget: allow the drop only if the drag carries at least one Control Rig asset. */
    bool HandleAllowDrop(TSharedPtr<FDragDropOperation> DragOperation) const;

    /** SDropTarget: a drop happened - filter by Control Rig type, append (de-duplicated) to the list. */
    FReply HandleDropped(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent);

    /** Generate one row in the asset list view. */
    TSharedRef<ITableRow> MakeAssetRowWidget(TSharedPtr<FAssetData> Item, const TSharedRef<STableViewBase>& OwnerTable);

    /** Remove a single asset row (the per-row × button). */
    FReply OnRemoveAssetClicked(TSharedPtr<FAssetData> Item);

    FReply OnClearClicked();
    FReply OnExportClicked();

    /** Tick one asset per frame until the queue is empty. */
    EActiveTimerReturnType StepExport(double CurrentTime, float DeltaSeconds);

    TOptional<float> GetProgressFraction() const;
    bool IsExportEnabled() const { return !bIsExporting && DraggedAssets.Num() > 0; }

    static FString AssetKey(const FAssetData& InAsset);

    // ---- UI controls ----
    TSharedPtr<SListView<TSharedPtr<FAssetData>>> ListView;
    TSharedPtr<SProgressBar> ProgressBar;
    TSharedPtr<STextBlock> StatusText;
    TSharedPtr<SButton> ExportButton;

    // ---- accepted source assets (shared pointers so SListView can hold them) ----
    TArray<TSharedPtr<FAssetData>> DraggedAssets;

    // ---- export state ----
    bool bIsExporting = false;
    int32 ExportedCount = 0;
    int32 TotalCount = 0;
    TArray<FAssetData> PendingAssets;
    TArray<TSharedPtr<FJsonObject>> IndexEntries;
    FString CurrentOutputDir;
    TWeakPtr<FActiveTimerHandle> TimerHandle;
};
