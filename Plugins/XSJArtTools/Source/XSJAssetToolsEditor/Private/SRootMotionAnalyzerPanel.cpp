#include "SRootMotionAnalyzerPanel.h"

#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetData.h"
#include "AssetSelection.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateColor.h"
#include "SDropTarget.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"

#define LOCTEXT_NAMESPACE "SRootMotionAnalyzerPanel"

namespace RootMotionAnalyzer
{
	static constexpr double MinimumDisplacement = 0.01;

	static FText FormatNumber(const double Value, const int32 FractionalDigits = 2)
	{
		FNumberFormattingOptions Options;
		Options.SetMinimumFractionalDigits(FractionalDigits);
		Options.SetMaximumFractionalDigits(FractionalDigits);
		return FText::AsNumber(Value, &Options);
	}
}

void SRootMotionAnalyzerPanel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SDropTarget)
		.OnAllowDrop(this, &SRootMotionAnalyzerPanel::HandleAllowDrop)
		.OnDropped(this, &SRootMotionAnalyzerPanel::HandleDropped)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 8.0f, 8.0f, 4.0f)
			[
				SNew(SBorder)
				.Padding(12.0f)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("DropHint", "将一个或多个 AnimSequence 拖到这里"))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("DropHintSub", "只显示启用 Root Motion 且存在水平位移的动画"))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
				]
			]

			+ SVerticalBox::Slot().FillHeight(1.0f).Padding(8.0f, 0.0f, 8.0f, 4.0f)
			[
				SAssignNew(ListView, SListView<TSharedPtr<FRootMotionAnimationRow>>)
				.ListItemsSource(&Rows)
				.OnGenerateRow(this, &SRootMotionAnalyzerPanel::MakeAnimationRow)
				.HeaderRow
				(
					SNew(SHeaderRow)
					+ SHeaderRow::Column("AnimationName")
					.DefaultLabel(LOCTEXT("AnimationNameColumn", "动画名称"))
					.FillWidth(2.2f)
					+ SHeaderRow::Column("TotalFrames")
					.DefaultLabel(LOCTEXT("TotalFramesColumn", "总帧数"))
					.FixedWidth(90.0f)
					+ SHeaderRow::Column("Duration")
					.DefaultLabel(LOCTEXT("DurationColumn", "时长(s)"))
					.FixedWidth(90.0f)
					+ SHeaderRow::Column("Displacement")
					.DefaultLabel(LOCTEXT("DisplacementColumn", "位移(cm)"))
					.FixedWidth(110.0f)
					+ SHeaderRow::Column("AverageSpeed")
					.DefaultLabel(LOCTEXT("AverageSpeedColumn", "平均速度(cm/s)"))
					.FixedWidth(140.0f)
					+ SHeaderRow::Column("Remove")
					.DefaultLabel(LOCTEXT("RemoveColumn", ""))
					.FixedWidth(32.0f)
				)
			]

			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(8.0f, 0.0f, 8.0f, 4.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("ClearButton", "清空列表"))
				.IsEnabled_Lambda([this]() { return Rows.Num() > 0; })
				.OnClicked(this, &SRootMotionAnalyzerPanel::OnClearClicked)
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(8.0f, 0.0f, 8.0f, 8.0f)
			[
				SAssignNew(StatusText, STextBlock)
				.Text(LOCTEXT("StatusReady", "就绪"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		]
	];
}

bool SRootMotionAnalyzerPanel::HandleAllowDrop(TSharedPtr<FDragDropOperation> DragOperation) const
{
	if (!DragOperation.IsValid())
	{
		return false;
	}

	const TArray<FAssetData> DraggedAssets = AssetUtil::ExtractAssetDataFromDrag(DragOperation);
	for (const FAssetData& Asset : DraggedAssets)
	{
		if (IsAnimationSequence(Asset))
		{
			return true;
		}
	}

	return false;
}

FReply SRootMotionAnalyzerPanel::HandleDropped(
	const FGeometry& MyGeometry,
	const FDragDropEvent& DragDropEvent
)
{
	(void)MyGeometry;

	const TArray<FAssetData> DraggedAssets = AssetUtil::ExtractAssetDataFromDrag(DragDropEvent);
	int32 AddedCount = 0;
	int32 IgnoredCount = 0;

	for (const FAssetData& Asset : DraggedAssets)
	{
		if (!IsAnimationSequence(Asset) || !AddAnimation(Asset))
		{
			++IgnoredCount;
			continue;
		}

		++AddedCount;
	}

	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}

	if (StatusText.IsValid())
	{
		StatusText->SetText(FText::Format(
			LOCTEXT("StatusAdded", "已添加 {0} 个，忽略 {1} 个，当前显示 {2} 个"),
			AddedCount,
			IgnoredCount,
			Rows.Num()
		));
	}

	return FReply::Handled();
}

TSharedRef<ITableRow> SRootMotionAnalyzerPanel::MakeAnimationRow(
	TSharedPtr<FRootMotionAnimationRow> Item,
	const TSharedRef<STableViewBase>& OwnerTable
)
{
	return SNew(STableRow<TSharedPtr<FRootMotionAnimationRow>>, OwnerTable)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(2.2f).VAlign(VAlign_Center).Padding(4.0f, 2.0f)
		[
			SNew(STextBlock)
			.Text_Lambda([Item]()
			{
				return Item.IsValid() ? FText::FromString(Item->AnimationName) : FText::GetEmpty();
			})
			.ToolTipText_Lambda([Item]()
			{
				return Item.IsValid() ? FText::FromString(Item->AssetPath) : FText::GetEmpty();
			})
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 2.0f)
		[
			SNew(SBox)
			.WidthOverride(90.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([Item]()
				{
					return Item.IsValid() ? FText::AsNumber(Item->TotalFrames) : FText::GetEmpty();
				})
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 2.0f)
		[
			SNew(SBox)
			.WidthOverride(90.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([Item]()
				{
					return Item.IsValid() ? RootMotionAnalyzer::FormatNumber(Item->Duration) : FText::GetEmpty();
				})
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 2.0f)
		[
			SNew(SBox)
			.WidthOverride(110.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([Item]()
				{
					return Item.IsValid() ? RootMotionAnalyzer::FormatNumber(Item->Displacement) : FText::GetEmpty();
				})
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 2.0f)
		[
			SNew(SBox)
			.WidthOverride(140.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([Item]()
				{
					return Item.IsValid() ? RootMotionAnalyzer::FormatNumber(Item->AverageSpeed) : FText::GetEmpty();
				})
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(32.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("RemoveButton", "x"))
				.ButtonStyle(FAppStyle::Get(), "NoBorder")
				.ContentPadding(FMargin(2.0f, 0.0f))
				.OnClicked(this, &SRootMotionAnalyzerPanel::OnRemoveClicked, Item)
			]
		]
	];
}

FReply SRootMotionAnalyzerPanel::OnClearClicked()
{
	Rows.Reset();

	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}

	if (StatusText.IsValid())
	{
		StatusText->SetText(LOCTEXT("StatusCleared", "列表已清空"));
	}

	return FReply::Handled();
}

FReply SRootMotionAnalyzerPanel::OnRemoveClicked(TSharedPtr<FRootMotionAnimationRow> Item)
{
	if (Item.IsValid())
	{
		Rows.Remove(Item);
	}

	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}

	if (StatusText.IsValid())
	{
		StatusText->SetText(FText::Format(
			LOCTEXT("StatusRemoved", "已移除，当前显示 {0} 个"),
			Rows.Num()
		));
	}

	return FReply::Handled();
}

bool SRootMotionAnalyzerPanel::AddAnimation(const FAssetData& Asset)
{
	const FString NewAssetKey = AssetKey(Asset);
	for (const TSharedPtr<FRootMotionAnimationRow>& Existing : Rows)
	{
		if (Existing.IsValid() && Existing->AssetPath == NewAssetKey)
		{
			return false;
		}
	}

	UAnimSequence* AnimSequence = Cast<UAnimSequence>(Asset.GetAsset());
	if (!IsValid(AnimSequence) || !AnimSequence->HasRootMotion())
	{
		return false;
	}

	const double Duration = AnimSequence->GetPlayLength();
	if (Duration <= UE_DOUBLE_SMALL_NUMBER)
	{
		return false;
	}

	const FAnimExtractContext ExtractContext(0.0, true, FDeltaTimeRecord(), false);
	const FTransform RootMotion = AnimSequence->ExtractRootMotionFromRange(
		0.0,
		Duration,
		ExtractContext
	);

	const double Displacement = RootMotion.GetTranslation().Size2D();
	if (Displacement <= RootMotionAnalyzer::MinimumDisplacement)
	{
		return false;
	}

	TSharedPtr<FRootMotionAnimationRow> Row = MakeShared<FRootMotionAnimationRow>();
	Row->AssetPath = NewAssetKey;
	Row->AnimationName = Asset.AssetName.ToString();
	Row->TotalFrames = AnimSequence->GetNumberOfSampledKeys();
	Row->Duration = Duration;
	Row->Displacement = Displacement;
	Row->AverageSpeed = Displacement / Duration;
	Rows.Add(Row);

	return true;
}

bool SRootMotionAnalyzerPanel::IsAnimationSequence(const FAssetData& Asset)
{
	return Asset.AssetClassPath.GetAssetName() == UAnimSequence::StaticClass()->GetFName();
}

FString SRootMotionAnalyzerPanel::AssetKey(const FAssetData& Asset)
{
	return Asset.PackageName.ToString() + TEXT(".") + Asset.AssetName.ToString();
}

#undef LOCTEXT_NAMESPACE
