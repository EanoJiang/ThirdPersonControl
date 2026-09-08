// Copyright (c) 2026 Shane Beal. Licensed under the MIT License.

#include "FootContactCurvesModifier.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimCurveTypes.h"
#include "Animation/AnimData/CurveIdentifier.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "AnimPose.h"
#include "Curves/RichCurve.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogFootContactCurves, Log, All);

#define LOCTEXT_NAMESPACE "FootContactCurvesModifier"

namespace
{
	struct FFootContactData
	{
		TArray<float> FootZ;
		TArray<float> VerticalSpeed;
		TArray<float> ContactScore;
		TArray<bool> Contact;
		float GroundLevel = 0.0f;
	};

	void RemoveShortContactRuns(TArray<bool>& Contact, int32 MinimumContactFrames, int32& OutRemovedFrames)
	{
		OutRemovedFrames = 0;
		const int32 SampleCount = Contact.Num();
		const int32 MaximumShortRunLength = FMath::Max(0, MinimumContactFrames);

		int32 RunStart = INDEX_NONE;
		for (int32 Sample = 0; Sample <= SampleCount; ++Sample)
		{
			const bool bInContact = Sample < SampleCount && Contact[Sample];

			if (bInContact && RunStart == INDEX_NONE)
			{
				RunStart = Sample;
				continue;
			}

			if (!bInContact && RunStart != INDEX_NONE)
			{
				const int32 RunLength = Sample - RunStart;
				if (RunLength <= MaximumShortRunLength)
				{
					for (int32 Index = RunStart; Index < Sample; ++Index)
					{
						Contact[Index] = false;
					}
					OutRemovedFrames += RunLength;
				}
				RunStart = INDEX_NONE;
			}
		}
	}

	int32 CountContactFrames(const TArray<bool>& Contact)
	{
		int32 Count = 0;
		for (const bool bIsContact : Contact)
		{
			Count += bIsContact ? 1 : 0;
		}
		return Count;
	}

	void ResolveTransientOverlaps(
		TArray<bool>& LeftContact,
		TArray<bool>& RightContact,
		const TArray<float>& LeftScore,
		const TArray<float>& RightScore,
		int32 MinimumTerminalSupportFrames,
		int32& OutResolvedOverlapFrames,
		int32& OutPreservedTerminalOverlapFrames)
	{
		OutResolvedOverlapFrames = 0;
		OutPreservedTerminalOverlapFrames = 0;
		int32 LastActiveFoot = INDEX_NONE;
		const int32 SampleCount = FMath::Min(LeftContact.Num(), RightContact.Num());
		const int32 MinimumSustainedTerminalFrames = FMath::Max(1, MinimumTerminalSupportFrames);

		for (int32 Sample = 0; Sample < SampleCount;)
		{
			const bool bLeft = LeftContact[Sample];
			const bool bRight = RightContact[Sample];

			if (bLeft && bRight)
			{
				const int32 OverlapStart = Sample;
				while (Sample < SampleCount && LeftContact[Sample] && RightContact[Sample])
				{
					++Sample;
				}

				const int32 OverlapLength = Sample - OverlapStart;
				const bool bIsSustainedTerminalSupport =
					Sample == SampleCount && OverlapLength >= MinimumSustainedTerminalFrames;

				if (bIsSustainedTerminalSupport)
				{
					OutPreservedTerminalOverlapFrames += OverlapLength;
					LastActiveFoot = INDEX_NONE;
					continue;
				}

				for (int32 OverlapSample = OverlapStart; OverlapSample < Sample; ++OverlapSample)
				{
					const float LeftConfidence = LeftScore.IsValidIndex(OverlapSample)
						? LeftScore[OverlapSample]
						: TNumericLimits<float>::Max();
					const float RightConfidence = RightScore.IsValidIndex(OverlapSample)
						? RightScore[OverlapSample]
						: TNumericLimits<float>::Max();

					if (LeftConfidence < RightConfidence - KINDA_SMALL_NUMBER)
					{
						RightContact[OverlapSample] = false;
						LastActiveFoot = 0;
					}
					else if (RightConfidence < LeftConfidence - KINDA_SMALL_NUMBER)
					{
						LeftContact[OverlapSample] = false;
						LastActiveFoot = 1;
					}
					else if (LastActiveFoot == 0)
					{
						RightContact[OverlapSample] = false;
					}
					else
					{
						LeftContact[OverlapSample] = false;
						LastActiveFoot = 1;
					}

					++OutResolvedOverlapFrames;
				}
			}
			else if (bLeft)
			{
				LastActiveFoot = 0;
				++Sample;
			}
			else if (bRight)
			{
				LastActiveFoot = 1;
				++Sample;
			}
			else
			{
				LastActiveFoot = INDEX_NONE;
				++Sample;
			}
		}
	}
}

UFootContactCurvesModifier::UFootContactCurvesModifier()
{
}

void UFootContactCurvesModifier::OnApply_Implementation(UAnimSequence* AnimationSequence)
{
	Super::OnApply_Implementation(AnimationSequence);

	if (!AnimationSequence)
	{
		UE_LOG(LogFootContactCurves, Warning, TEXT("OnApply: null AnimationSequence."));
		return;
	}

	if (!bBakeLeftFoot && !bBakeRightFoot)
	{
		UE_LOG(LogFootContactCurves, Warning,
			TEXT("OnApply on '%s': both feet are disabled; nothing to bake."),
			*AnimationSequence->GetName());
		return;
	}

	// Disable root-motion lock during sampling so foot bones evaluate consistently.
	TGuardValue<bool> ForceRootLockGuard(AnimationSequence->bForceRootLock, false);

	BakeContactCurves(AnimationSequence);
}

void UFootContactCurvesModifier::OnRevert_Implementation(UAnimSequence* AnimationSequence)
{
	Super::OnRevert_Implementation(AnimationSequence);

	if (!AnimationSequence)
	{
		return;
	}

	IAnimationDataController& Controller = AnimationSequence->GetController();
	Controller.OpenBracket(LOCTEXT("RevertFootContactCurves", "Revert Foot Contact Curves"));

	if (bBakeLeftFoot)
	{
		Controller.RemoveCurve(FAnimationCurveIdentifier(CurveName_L, ERawCurveTrackTypes::RCT_Float));
	}
	if (bBakeRightFoot)
	{
		Controller.RemoveCurve(FAnimationCurveIdentifier(CurveName_R, ERawCurveTrackTypes::RCT_Float));
	}

	Controller.CloseBracket();
}

void UFootContactCurvesModifier::BakeContactCurves(UAnimSequence* AnimationSequence) const
{
	const bool bHasLeftFoot = bBakeLeftFoot && !FootBoneName_L.IsNone() && !CurveName_L.IsNone();
	const bool bHasRightFoot = bBakeRightFoot && !FootBoneName_R.IsNone() && !CurveName_R.IsNone();

	if (!bHasLeftFoot && !bHasRightFoot)
	{
		UE_LOG(LogFootContactCurves, Warning,
			TEXT("BakeContactCurves: no valid enabled foot bone and curve name on '%s'; skipping."),
			*GetNameSafe(AnimationSequence));
		return;
	}

	const float SequenceLength = AnimationSequence->GetPlayLength();
	if (SequenceLength <= 0.f || SampleRate <= 0)
	{
		UE_LOG(LogFootContactCurves, Warning,
			TEXT("BakeContactCurves: invalid sequence length (%.3f) or sample rate (%d) on '%s'."),
			SequenceLength, SampleRate, *GetNameSafe(AnimationSequence));
		return;
	}

	const float SampleStep = 1.0f / FMath::Max(1.0f, static_cast<float>(SampleRate));
	const int32 SampleCount = FMath::Max(2, FMath::TruncToInt(SequenceLength / SampleStep) + 1);

	FAnimPoseEvaluationOptions EvalOptions;
	EvalOptions.EvaluationType = EAnimDataEvalType::Raw;
	EvalOptions.bShouldRetarget = true;
	EvalOptions.bExtractRootMotion = false;
	EvalOptions.bIncorporateRootMotionIntoPose = false;
	EvalOptions.bRetrieveAdditiveAsFullPose = true;
	EvalOptions.bEvaluateCurves = false;

	// Pass 1: sample both feet in one pose evaluation pass. Each foot gets its own
	// baseline because the two foot bones can have different rest offsets.
	FFootContactData LeftData;
	FFootContactData RightData;
	LeftData.FootZ.SetNumZeroed(SampleCount);
	RightData.FootZ.SetNumZeroed(SampleCount);
	float LeftGroundLevel = TNumericLimits<float>::Max();
	float RightGroundLevel = TNumericLimits<float>::Max();

	for (int32 Sample = 0; Sample < SampleCount; ++Sample)
	{
		const float Time = FMath::Clamp(static_cast<float>(Sample) * SampleStep, 0.0f, SequenceLength);

		FAnimPose Pose;
		UAnimPoseExtensions::GetAnimPoseAtTime(AnimationSequence, Time, EvalOptions, Pose);

		if (!Pose.IsValid())
		{
			UE_LOG(LogFootContactCurves, Warning,
				TEXT("BakeContactCurve: invalid pose at time %.3f on '%s'; aborting."),
				Time, *GetNameSafe(AnimationSequence));
			return;
		}

		if (bHasLeftFoot)
		{
			const FTransform LeftFootTransform = UAnimPoseExtensions::GetBonePose(Pose, FootBoneName_L, EAnimPoseSpaces::World);
			LeftData.FootZ[Sample] = LeftFootTransform.GetLocation().Z;
			LeftGroundLevel = FMath::Min(LeftGroundLevel, LeftData.FootZ[Sample]);
		}

		if (bHasRightFoot)
		{
			const FTransform RightFootTransform = UAnimPoseExtensions::GetBonePose(Pose, FootBoneName_R, EAnimPoseSpaces::World);
			RightData.FootZ[Sample] = RightFootTransform.GetLocation().Z;
			RightGroundLevel = FMath::Min(RightGroundLevel, RightData.FootZ[Sample]);
		}
	}

	LeftData.GroundLevel = bHasLeftFoot ? LeftGroundLevel : 0.0f;
	RightData.GroundLevel = bHasRightFoot ? RightGroundLevel : 0.0f;

	auto BuildContactData = [this, SampleCount, SampleStep](FFootContactData& Data)
	{
		Data.VerticalSpeed.SetNumZeroed(SampleCount);
		Data.ContactScore.SetNumZeroed(SampleCount);
		Data.Contact.SetNumZeroed(SampleCount);

		for (int32 Sample = 0; Sample < SampleCount; ++Sample)
		{
			if (Sample == 0 && SampleCount > 1)
			{
				Data.VerticalSpeed[Sample] = FMath::Abs(Data.FootZ[1] - Data.FootZ[0]) / SampleStep;
			}
			else if (Sample > 0)
			{
				Data.VerticalSpeed[Sample] = FMath::Abs(Data.FootZ[Sample] - Data.FootZ[Sample - 1]) / SampleStep;
			}

			const float HeightAboveGround = FMath::Max(0.f, Data.FootZ[Sample] - Data.GroundLevel);
			const float HeightScore = HeightAboveGround / FMath::Max(GroundHeightThreshold, KINDA_SMALL_NUMBER);
			const float SpeedScore = Data.VerticalSpeed[Sample] / FMath::Max(VerticalSpeedThreshold, KINDA_SMALL_NUMBER);
			Data.ContactScore[Sample] = HeightScore + SpeedScore;
			Data.Contact[Sample] = (HeightAboveGround < GroundHeightThreshold) && (Data.VerticalSpeed[Sample] < VerticalSpeedThreshold);
		}
	};

	if (bHasLeftFoot)
	{
		BuildContactData(LeftData);
	}
	if (bHasRightFoot)
	{
		BuildContactData(RightData);
	}

	int32 RemovedLeftFrames = 0;
	int32 RemovedRightFrames = 0;
	if (bHasLeftFoot)
	{
		RemoveShortContactRuns(LeftData.Contact, MinimumContactFrames, RemovedLeftFrames);
	}
	if (bHasRightFoot)
	{
		RemoveShortContactRuns(RightData.Contact, MinimumContactFrames, RemovedRightFrames);
	}

	int32 ResolvedOverlapFrames = 0;
	int32 PreservedTerminalOverlapFrames = 0;
	if (bHasLeftFoot && bHasRightFoot && bEnforceExclusiveFootContacts)
	{
		ResolveTransientOverlaps(
			LeftData.Contact,
			RightData.Contact,
			LeftData.ContactScore,
			RightData.ContactScore,
			MinimumContactFrames,
			ResolvedOverlapFrames,
			PreservedTerminalOverlapFrames);
	}

	auto WriteCurve = [this, AnimationSequence, SequenceLength, SampleStep, SampleCount](FName CurveName, const TArray<bool>& Contact)
	{
		TArray<FRichCurveKey> Keys;
		Keys.Reserve(SampleCount);

		for (int32 Sample = 0; Sample < SampleCount; ++Sample)
		{
			const float Time = FMath::Clamp(static_cast<float>(Sample) * SampleStep, 0.0f, SequenceLength);
			const float Value = Contact[Sample] ? 1.0f : 0.0f;

			FRichCurveKey Key(Time, Value);
			Key.InterpMode = InterpolationMode;
			Keys.Add(Key);
		}

		IAnimationDataController& Controller = AnimationSequence->GetController();
		const FAnimationCurveIdentifier CurveId(CurveName, ERawCurveTrackTypes::RCT_Float);

		if (bOverwriteExistingCurves)
		{
			Controller.RemoveCurve(CurveId);
		}

		Controller.AddCurve(CurveId, AACF_DefaultCurve);
		Controller.SetCurveKeys(CurveId, Keys);
	};

	IAnimationDataController& Controller = AnimationSequence->GetController();
	Controller.OpenBracket(LOCTEXT("BakeFootContactCurves", "Bake Foot Contact Curves"));

	if (bHasLeftFoot)
	{
		WriteCurve(CurveName_L, LeftData.Contact);
	}
	if (bHasRightFoot)
	{
		WriteCurve(CurveName_R, RightData.Contact);
	}

	Controller.CloseBracket();

	auto LogBakeStats = [AnimationSequence, SampleCount, this, ResolvedOverlapFrames, PreservedTerminalOverlapFrames](
		FName CurveName,
		const TArray<bool>& Contact,
		int32 RemovedFrames,
		float GroundLevel)
	{
		const int32 PlantedFrames = CountContactFrames(Contact);
		UE_LOG(LogFootContactCurves, Display,
			TEXT("Baked '%s' on '%s': %d samples, %d planted (%.1f%%), removed_short=%d, resolved_overlap=%d, preserved_terminal_overlap=%d, ground=%.2fcm, height_thr=%.2fcm, vspeed_thr=%.2fcm/s."),
			*CurveName.ToString(),
			*AnimationSequence->GetName(),
			SampleCount,
			PlantedFrames,
			100.f * static_cast<float>(PlantedFrames) / FMath::Max(1, SampleCount),
			RemovedFrames,
			ResolvedOverlapFrames,
			PreservedTerminalOverlapFrames,
			GroundLevel,
			GroundHeightThreshold,
			VerticalSpeedThreshold);
	};

	if (bHasLeftFoot)
	{
		LogBakeStats(CurveName_L, LeftData.Contact, RemovedLeftFrames, LeftData.GroundLevel);
	}
	if (bHasRightFoot)
	{
		LogBakeStats(CurveName_R, RightData.Contact, RemovedRightFrames, RightData.GroundLevel);
	}
}

#undef LOCTEXT_NAMESPACE
