#include "SControlRigToJsonExportPanel.h"

#include "AssetRegistry/AssetData.h"
#include "ControlRigToJsonExporter.h"
#include "DesktopPlatformModule.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformFileManager.h"
#include "IDesktopPlatform.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SControlRigToJsonExportPanel"

void SControlRigToJsonExportPanel::Construct(const FArguments& InArgs)
{
    ChildSlot
    [
        SNew(SVerticalBox)

        // Input directory
        + SVerticalBox::Slot().AutoHeight().Padding(8.0f, 8.0f, 8.0f, 2.0f)
        [
            SNew(STextBlock).Text(LOCTEXT("InputHint", "Source Content folder (e.g., /Game/Data/source/player/F2/Character)"))
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(8.0f, 0.0f, 8.0f, 8.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f)
            [
                SAssignNew(InputTextBox, SEditableTextBox)
                .HintText(LOCTEXT("InputHintPlaceholder", "Select or paste absolute Content subfolder path"))
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("BrowseInput", "Browse..."))
                .OnClicked(this, &SControlRigToJsonExportPanel::OnBrowseInputClicked)
            ]
        ]

        // Output directory
        + SVerticalBox::Slot().AutoHeight().Padding(8.0f, 0.0f, 8.0f, 2.0f)
        [
            SNew(STextBlock).Text(LOCTEXT("OutputHint", "Output directory (absolute path for JSON files)"))
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(8.0f, 0.0f, 8.0f, 8.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f)
            [
                SAssignNew(OutputTextBox, SEditableTextBox)
                .HintText(LOCTEXT("OutputHintPlaceholder", "Select or paste output folder absolute path"))
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("BrowseOutput", "Browse..."))
                .OnClicked(this, &SControlRigToJsonExportPanel::OnBrowseOutputClicked)
            ]
        ]

        // Export button
        + SVerticalBox::Slot().AutoHeight().Padding(8.0f, 4.0f, 8.0f, 4.0f)
        [
            SAssignNew(ExportButton, SButton)
            .Text(LOCTEXT("ExportButton", "Export"))
            .HAlign(HAlign_Center)
            .OnClicked(this, &SControlRigToJsonExportPanel::OnExportClicked)
            .IsEnabled(this, &SControlRigToJsonExportPanel::IsExportEnabled)
        ]

        // Progress bar
        + SVerticalBox::Slot().AutoHeight().Padding(8.0f, 4.0f, 8.0f, 2.0f)
        [
            SAssignNew(ProgressBar, SProgressBar)
            .Percent(this, &SControlRigToJsonExportPanel::GetProgressFraction)
        ]

        // Status text
        + SVerticalBox::Slot().AutoHeight().Padding(8.0f, 0.0f, 8.0f, 8.0f)
        [
            SAssignNew(StatusText, STextBlock)
            .Text(LOCTEXT("StatusIdle", "Ready"))
        ]
    ];
}

FReply SControlRigToJsonExportPanel::OnBrowseInputClicked()
{
    FString DefaultPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
    FString Chosen;
    if (FControlRigToJsonExporter::PickFolder(LOCTEXT("PickInputFolder", "Select source Content folder"), DefaultPath, Chosen))
    {
        if (InputTextBox.IsValid())
        {
            InputTextBox->SetText(FText::FromString(Chosen));
        }
    }
    return FReply::Handled();
}

FReply SControlRigToJsonExportPanel::OnBrowseOutputClicked()
{
    FString DefaultPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FString Chosen;
    if (FControlRigToJsonExporter::PickFolder(LOCTEXT("PickOutputFolder", "Select output directory"), DefaultPath, Chosen))
    {
        if (OutputTextBox.IsValid())
        {
            OutputTextBox->SetText(FText::FromString(Chosen));
        }
    }
    return FReply::Handled();
}

FReply SControlRigToJsonExportPanel::OnExportClicked()
{
    const FString InputAbs = InputTextBox.IsValid() ? InputTextBox->GetText().ToString().TrimStartAndEnd() : FString();
    const FString OutputDir = OutputTextBox.IsValid() ? OutputTextBox->GetText().ToString().TrimStartAndEnd() : FString();

    if (InputAbs.IsEmpty() || OutputDir.IsEmpty())
    {
        if (StatusText.IsValid())
        {
            StatusText->SetText(LOCTEXT("StatusMissingPaths", "Please fill in both source and output directories."));
        }
        FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("MissingFields", "Please fill in both source and output directories."));
        return FReply::Handled();
    }

    FString GamePath;
    FString Error;
    if (!FControlRigToJsonExporter::ContentFolderToGamePath(InputAbs, GamePath, Error))
    {
        if (StatusText.IsValid())
        {
            StatusText->SetText(FText::FromString(Error));
        }
        FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Error));
        return FReply::Handled();
    }

    // Gather all Control Rig assets before the per-frame export
    PendingAssets.Reset();
    if (!FControlRigToJsonExporter::Get().GatherControlRigAssets(GamePath, PendingAssets))
    {
        const FText Msg = LOCTEXT("AssetRegistryLoading", "Asset registry is still loading. Please try again in a moment.");
        if (StatusText.IsValid())
        {
            StatusText->SetText(Msg);
        }
        FMessageDialog::Open(EAppMsgType::Ok, Msg);
        return FReply::Handled();
    }

    TotalCount = PendingAssets.Num();
    if (TotalCount == 0)
    {
        const FText Msg = FText::Format(LOCTEXT("NoControlRigs", "No Control Rig assets found under {0}"), FText::FromString(GamePath));
        if (StatusText.IsValid())
        {
            StatusText->SetText(Msg);
        }
        FMessageDialog::Open(EAppMsgType::Ok, Msg);
        return FReply::Handled();
    }

    // Start per-frame export
    CurrentInputGamePath = GamePath;
    CurrentOutputDir = OutputDir;
    ExportedCount = 0;
    IndexEntries.Reset();
    bIsExporting = true;
    if (StatusText.IsValid())
    {
        StatusText->SetText(FText::Format(LOCTEXT("StatusStarting", "Exporting... 0/{0}"), TotalCount));
    }

    RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SControlRigToJsonExportPanel::StepExport));
    return FReply::Handled();
}

EActiveTimerReturnType SControlRigToJsonExportPanel::StepExport(double CurrentTime, float DeltaSeconds)
{
    if (!bIsExporting || PendingAssets.Num() == 0)
    {
        bIsExporting = false;
        return EActiveTimerReturnType::Stop;
    }

    const FAssetData Asset = PendingAssets.Pop(EAllowShrinking::No);

    TSharedPtr<FJsonObject> Entry;
    FString FileName;
    if (FControlRigToJsonExporter::Get().ExportSingleAsset(Asset, CurrentOutputDir, Entry, FileName) && Entry.IsValid())
    {
        IndexEntries.Add(Entry);
        ++ExportedCount;
    }

    if (StatusText.IsValid())
    {
        StatusText->SetText(FText::Format(LOCTEXT("StatusProgress", "Exporting... {0}/{1}"), ExportedCount, TotalCount));
    }

    if (PendingAssets.Num() == 0)
    {
        FControlRigToJsonExporter::Get().WriteIndexJson(IndexEntries, CurrentInputGamePath, CurrentOutputDir);
        bIsExporting = false;

        if (StatusText.IsValid())
        {
            StatusText->SetText(FText::Format(LOCTEXT("StatusDone", "Done! Exported {0} Control Rig(s)."), ExportedCount));
        }
        FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("DoneMessage", "Export complete! {0} Control Rig(s) exported."), ExportedCount));
        return EActiveTimerReturnType::Stop;
    }

    return EActiveTimerReturnType::Continue;
}

TOptional<float> SControlRigToJsonExportPanel::GetProgressFraction() const
{
    if (TotalCount <= 0)
    {
        return 0.0f;
    }
    return static_cast<float>(ExportedCount) / static_cast<float>(TotalCount);
}

#undef LOCTEXT_NAMESPACE
