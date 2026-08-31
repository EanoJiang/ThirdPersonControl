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
 * Control Rig export panel (Slate widget).
 * Select a source Content directory + an output directory, then export each
 * Control Rig under that source to a separate JSON file.
 * The export is driven by an ActiveTimer so the progress bar updates and the
 * window stays responsive.
 */
class SControlRigToJsonExportPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SControlRigToJsonExportPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    FReply OnBrowseInputClicked();
    FReply OnBrowseOutputClicked();
    FReply OnExportClicked();

    /** Tick one asset per frame until the queue is empty. */
    EActiveTimerReturnType StepExport(double CurrentTime, float DeltaSeconds);

    TOptional<float> GetProgressFraction() const;
    bool IsExportEnabled() const { return !bIsExporting; }

    // ---- UI controls ----
    TSharedPtr<SEditableTextBox> InputTextBox;
    TSharedPtr<SEditableTextBox> OutputTextBox;
    TSharedPtr<SProgressBar> ProgressBar;
    TSharedPtr<STextBlock> StatusText;
    TSharedPtr<SButton> ExportButton;

    // ---- export state ----
    bool bIsExporting = false;
    int32 ExportedCount = 0;
    int32 TotalCount = 0;
    TArray<FAssetData> PendingAssets;
    TArray<TSharedPtr<FJsonObject>> IndexEntries;
    FString CurrentInputGamePath;
    FString CurrentOutputDir;
    TWeakPtr<FActiveTimerHandle> TimerHandle;
};
