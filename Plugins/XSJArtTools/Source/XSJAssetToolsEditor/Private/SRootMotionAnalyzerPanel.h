#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

struct FAssetData;
class FDragDropOperation;
class STextBlock;
class STableViewBase;

template <typename ItemType>
class SListView;

struct FRootMotionAnimationRow
{
	FString AssetPath;
	FString AnimationName;
	int32 TotalFrames = 0;
	double Duration = 0.0;
	double Displacement = 0.0;
	double AverageSpeed = 0.0;
};

class SRootMotionAnalyzerPanel final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRootMotionAnalyzerPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	bool HandleAllowDrop(TSharedPtr<FDragDropOperation> DragOperation) const;
	FReply HandleDropped(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent);

	TSharedRef<ITableRow> MakeAnimationRow(
		TSharedPtr<FRootMotionAnimationRow> Item,
		const TSharedRef<STableViewBase>& OwnerTable
	);

	FReply OnClearClicked();
	FReply OnRemoveClicked(TSharedPtr<FRootMotionAnimationRow> Item);

	bool AddAnimation(const FAssetData& Asset);
	static bool IsAnimationSequence(const FAssetData& Asset);
	static FString AssetKey(const FAssetData& Asset);

	TSharedPtr<SListView<TSharedPtr<FRootMotionAnimationRow>>> ListView;
	TSharedPtr<STextBlock> StatusText;
	TArray<TSharedPtr<FRootMotionAnimationRow>> Rows;
};
