// 版权所有 Epic Games, Inc. 保留所有权利。

#include "SBlueprintToJsonExportPanel.h"

#include "AssetRegistry/AssetData.h"
#include "BlueprintToJsonExporter.h"
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

#define LOCTEXT_NAMESPACE "SBlueprintToJsonExportPanel"

void SBlueprintToJsonExportPanel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)

		// 源（输入）目录。
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 8.0f, 8.0f, 2.0f)
		[
			SNew(STextBlock).Text(LOCTEXT("InputHint", "原始目录（Content 子目录，如 /Game/UltraDynamicSky 对应的磁盘路径）"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 0.0f, 8.0f, 8.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SAssignNew(InputTextBox, SEditableTextBox)
				.HintText(LOCTEXT("InputHintPlaceholder", "选择或粘贴 Content 下的文件夹绝对路径"))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("BrowseInput", "浏览…"))
				.OnClicked(this, &SBlueprintToJsonExportPanel::OnBrowseInputClicked)
			]
		]

		// 输出目录。
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 0.0f, 8.0f, 2.0f)
		[
			SNew(STextBlock).Text(LOCTEXT("OutputHint", "导出目录（JSON 输出文件夹绝对路径）"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 0.0f, 8.0f, 8.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SAssignNew(OutputTextBox, SEditableTextBox)
				.HintText(LOCTEXT("OutputHintPlaceholder", "选择或粘贴输出文件夹绝对路径"))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("BrowseOutput", "浏览…"))
				.OnClicked(this, &SBlueprintToJsonExportPanel::OnBrowseOutputClicked)
			]
		]

		// 导出按钮。
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 4.0f, 8.0f, 4.0f)
		[
			SAssignNew(ExportButton, SButton)
			.Text(LOCTEXT("ExportButton", "导出"))
			.HAlign(HAlign_Center)
			.OnClicked(this, &SBlueprintToJsonExportPanel::OnExportClicked)
			.IsEnabled(this, &SBlueprintToJsonExportPanel::IsExportEnabled)
		]

		// 进度条。
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 4.0f, 8.0f, 2.0f)
		[
			SAssignNew(ProgressBar, SProgressBar)
			.Percent(this, &SBlueprintToJsonExportPanel::GetProgressFraction)
		]

		// 状态文本。
		+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 0.0f, 8.0f, 8.0f)
		[
			SAssignNew(StatusText, STextBlock)
			.Text(LOCTEXT("StatusIdle", "待命"))
		]
	];
}

FReply SBlueprintToJsonExportPanel::OnBrowseInputClicked()
{
	FString DefaultPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	FString Chosen;
	if (FBlueprintToJsonExporter::PickFolder(LOCTEXT("PickInputFolder", "选择原始目录（Content 子目录）"), DefaultPath, Chosen))
	{
		if (InputTextBox.IsValid())
		{
			InputTextBox->SetText(FText::FromString(Chosen));
		}
	}
	return FReply::Handled();
}

FReply SBlueprintToJsonExportPanel::OnBrowseOutputClicked()
{
	FString DefaultPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FString Chosen;
	if (FBlueprintToJsonExporter::PickFolder(LOCTEXT("PickOutputFolder", "选择导出目录"), DefaultPath, Chosen))
	{
		if (OutputTextBox.IsValid())
		{
			OutputTextBox->SetText(FText::FromString(Chosen));
		}
	}
	return FReply::Handled();
}

FReply SBlueprintToJsonExportPanel::OnExportClicked()
{
	const FString InputAbs = InputTextBox.IsValid() ? InputTextBox->GetText().ToString().TrimStartAndEnd() : FString();
	const FString OutputDir = OutputTextBox.IsValid() ? OutputTextBox->GetText().ToString().TrimStartAndEnd() : FString();

	if (InputAbs.IsEmpty() || OutputDir.IsEmpty())
	{
		if (StatusText.IsValid())
		{
			StatusText->SetText(LOCTEXT("StatusMissingPaths", "请填写原始目录和导出目录"));
		}
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("MissingFields", "请填写原始目录和导出目录。"));
		return FReply::Handled();
	}

	FString GamePath;
	FString Error;
	if (!FBlueprintToJsonExporter::ContentFolderToGamePath(InputAbs, GamePath, Error))
	{
		if (StatusText.IsValid())
		{
			StatusText->SetText(FText::FromString(Error));
		}
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Error));
		return FReply::Handled();
	}

	// 提前收集所有蓝图资产（必须在启动逐帧导出前完成）。
	PendingAssets.Reset();
	if (!FBlueprintToJsonExporter::Get().GatherBlueprintAssets(GamePath, PendingAssets))
	{
		const FText Msg = LOCTEXT("AssetRegistryLoading", "资产注册表仍在加载，请稍后再试。");
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
		const FText Msg = FText::Format(LOCTEXT("NoBlueprints", "在 {0} 下未找到蓝图。"), FText::FromString(GamePath));
		if (StatusText.IsValid())
		{
			StatusText->SetText(Msg);
		}
		FMessageDialog::Open(EAppMsgType::Ok, Msg);
		return FReply::Handled();
	}

	// 启动逐帧导出。
	CurrentInputGamePath = GamePath;
	CurrentOutputDir = OutputDir;
	ExportedCount = 0;
	IndexEntries.Reset();
	bIsExporting = true;
	if (StatusText.IsValid())
	{
		StatusText->SetText(FText::Format(LOCTEXT("StatusStarting", "导出中 0/{0}"), TotalCount));
	}

	RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SBlueprintToJsonExportPanel::StepExport));
	return FReply::Handled();
}

EActiveTimerReturnType SBlueprintToJsonExportPanel::StepExport(double CurrentTime, float DeltaSeconds)
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
		StatusText->SetText(FText::Format(LOCTEXT("StatusProgress", "导出中 {0}/{1}"), ExportedCount, TotalCount));
	}

	// 队列清空：收尾并停止定时器。
	if (PendingAssets.Num() == 0)
	{
		FBlueprintToJsonExporter::Get().WriteIndexJson(IndexEntries, CurrentInputGamePath, CurrentOutputDir);
		bIsExporting = false;

		if (StatusText.IsValid())
		{
			StatusText->SetText(FText::Format(LOCTEXT("StatusDone", "完成，共导出 {0} 个蓝图"), ExportedCount));
		}
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("DoneMessage", "导出完成，共 {0} 个蓝图。"), ExportedCount));
		return EActiveTimerReturnType::Stop;
	}

	return EActiveTimerReturnType::Continue;
}

TOptional<float> SBlueprintToJsonExportPanel::GetProgressFraction() const
{
	if (TotalCount <= 0)
	{
		return 0.0f;
	}
	return static_cast<float>(ExportedCount) / static_cast<float>(TotalCount);
}

#undef LOCTEXT_NAMESPACE
