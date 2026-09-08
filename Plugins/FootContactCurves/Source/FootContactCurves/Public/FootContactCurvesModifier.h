// Copyright (c) 2026 Shane Beal. Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "AnimationModifier.h"

#include "FootContactCurvesModifier.generated.h"

class UAnimSequence;

/**
 * Animation Modifier that bakes per-foot contact curves (default `contact_l` and `contact_r`)
 * onto an AnimSequence. Compatible with Game Animation Sample Project (GASP), motion matching,
 * foot-lock IK, and any pipeline that consumes 0/1 per-foot contact gates.
 *
 * Algorithm:
 *  - Sample both enabled foot bones in one pass and keep an independent ground level for each foot.
 *  - Per frame, mark a foot as planted when both
 *      `(foot_height_above_its_ground < GroundHeightThreshold)` AND
 *      `(foot_vertical_speed < VerticalSpeedThreshold)`.
 *  - Remove short contact islands and, for walking-style data, resolve transient
 *    left/right overlap. A sustained overlap that reaches the final sample is kept
 *    so a stop animation can end in a double-support pose.
 *  - Writes the curves through `IAnimationDataController::SetCurveKeys` with the chosen
 *    interpolation mode.
 *
 * Usage: select one or more AnimSequences in the Content Browser → right-click →
 * Animation Asset Actions → Apply Animation Modifier → Foot Contact Curves.
 */
UCLASS(meta = (DisplayName = "Foot Contact Curves"))
class FOOTCONTACTCURVES_API UFootContactCurvesModifier : public UAnimationModifier
{
	GENERATED_BODY()

public:
	UFootContactCurvesModifier();

	UPROPERTY(EditAnywhere, Category = "Left Foot")
	bool bBakeLeftFoot = true;

	UPROPERTY(EditAnywhere, Category = "Left Foot", meta = (EditCondition = "bBakeLeftFoot"))
	FName FootBoneName_L = TEXT("foot_l");

	UPROPERTY(EditAnywhere, Category = "Left Foot", meta = (EditCondition = "bBakeLeftFoot"))
	FName CurveName_L = TEXT("contact_l");

	UPROPERTY(EditAnywhere, Category = "Right Foot")
	bool bBakeRightFoot = true;

	UPROPERTY(EditAnywhere, Category = "Right Foot", meta = (EditCondition = "bBakeRightFoot"))
	FName FootBoneName_R = TEXT("foot_r");

	UPROPERTY(EditAnywhere, Category = "Right Foot", meta = (EditCondition = "bBakeRightFoot"))
	FName CurveName_R = TEXT("contact_r");

	/** Frames-per-second to sample the animation at. Higher = more accurate, slower. */
	UPROPERTY(EditAnywhere, Category = "Sampling", meta = (ClampMin = "10", ClampMax = "240"))
	int32 SampleRate = 30;

	/** A foot is considered planted when its height above that foot's lowest Z over the clip is below this many centimeters. */
	UPROPERTY(EditAnywhere, Category = "Thresholds", meta = (ClampMin = "0.1"))
	float GroundHeightThreshold = 6.0f;

	/** A foot is considered planted when its vertical speed is below this many cm/sec. */
	UPROPERTY(EditAnywhere, Category = "Thresholds", meta = (ClampMin = "0.0"))
	float VerticalSpeedThreshold = 30.0f;

	/** Curve key interpolation: Linear gives smooth ramps between 0 and 1; Constant gives sharp on/off plateaus; Cubic gives soft eased transitions. */
	UPROPERTY(EditAnywhere, Category = "Output")
	TEnumAsByte<ERichCurveInterpMode> InterpolationMode = RCIM_Linear;

	/** Removes contact runs this short or shorter. At 30 fps, the default removes brief 1-4 frame false positives. */
	UPROPERTY(EditAnywhere, Category = "Output", meta = (ClampMin = "1", ClampMax = "30"))
	int32 MinimumContactFrames = 4;

	/** Resolves transient walking overlap between contact_l and contact_r. Sustained overlap at the end of the clip is preserved for double support. */
	UPROPERTY(EditAnywhere, Category = "Output")
	bool bEnforceExclusiveFootContacts = true;

	/** If true, any existing curves with the configured names are removed before baking. If false, baking aborts when the curves already exist. */
	UPROPERTY(EditAnywhere, Category = "Output")
	bool bOverwriteExistingCurves = true;

	virtual void OnApply_Implementation(UAnimSequence* AnimationSequence) override;
	virtual void OnRevert_Implementation(UAnimSequence* AnimationSequence) override;

private:
	void BakeContactCurves(UAnimSequence* AnimationSequence) const;
};
