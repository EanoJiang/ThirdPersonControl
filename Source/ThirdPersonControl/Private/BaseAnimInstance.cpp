// Fill out your copyright notice in the Description page of Project Settings.


#include "BaseAnimInstance.h"
#include "BaseController.h"
#include "CanvasItem.h"
#include "Debug/DebugDrawService.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "SceneInterface.h"
#include "SceneView.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/KismetMathLibrary.h"

#pragma region DrawDebugMessages

void UBaseAnimInstance::DrawDebugKeyValueMessages(
	const FName Key,
	const FString& Value,
	const FLinearColor KeyColor,
	const FLinearColor ValueColor,
	const FVector LocationOffset)
{
#if ENABLE_ANIM_DRAW_DEBUG
	if (!ensureMsgf(IsInGameThread(), TEXT("Draw Debug Key Value must run on the game thread.")))
	{
		return;
	}
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	const AActor* Owner = Mesh ? Mesh->GetOwner() : nullptr;
	if (!Owner)
	{
		return;
	}

	FVector DrawLocationOffset = LocationOffset;
	if (const APawn* PawnOwner = TryGetPawnOwner())
	{
		if (const APlayerController* PlayerController =
			Cast<APlayerController>(PawnOwner->GetController()))
		{
			FVector ViewLocation;
			FRotator ViewRotation;
			PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

			// XY 跟随完整相机旋转：X 沿视线前后，Y 沿相机左右。
			// Z 单独叠加，继续保持世界向上。
			DrawLocationOffset = ViewRotation.RotateVector(
				FVector(LocationOffset.X, LocationOffset.Y, 0.0f))
				+ FVector(0.0f, 0.0f, LocationOffset.Z);
		}
	}

	if (DebugKeyValueFrame != GFrameCounter)
	{
		DebugKeyValueMessages.Reset();
		DebugKeyValueFrame = GFrameCounter;
	}
	DebugKeyValueMessages.Add({Key.ToString() + TEXT(":"), Value,
		KeyColor, ValueColor, Owner->GetActorLocation() + DrawLocationOffset});
	if (!DebugKeyValueDrawHandle.IsValid())
	{
		DebugKeyValueDrawHandle = UDebugDrawService::Register(TEXT("Game"),
			FDebugDrawDelegate::CreateUObject(this, &UBaseAnimInstance::DrawDebugKeyValueMessages));
	}
#endif
}

void UBaseAnimInstance::DrawDebugKeyValueMessages(UCanvas* Canvas, APlayerController* PlayerController)
{
#if ENABLE_ANIM_DRAW_DEBUG
	// Debug draw can be called for other editor/PIE worlds. Never leak text into those views.
	if (!Canvas || !Canvas->Canvas || !Canvas->SceneView || !GEngine
		|| !Canvas->SceneView->Family || !Canvas->SceneView->Family->Scene
		|| Canvas->SceneView->Family->Scene->GetWorld() != GetWorld()
		|| DebugKeyValueFrame == MAX_uint64 || GFrameCounter - DebugKeyValueFrame > 1)
	{
		return;
	}
	UFont* Font = GEngine->GetSmallFont();
	if (!Font)
	{
		return;
	}
	// Reset for each viewport draw; stack this instance's messages in submission order.
	float LineOffsetY = 0.0f;
	constexpr float LineSpacing = 4.0f;
	for (const FDebugKeyValueMessage& Message : DebugKeyValueMessages)
	{
		if (Canvas->SceneView->WorldToScreen(Message.WorldLocation).W <= 0.0f)
		{
			continue;
		}
		const FVector ScreenLocation = Canvas->Project(Message.WorldLocation);
		const FVector2D TextPosition(ScreenLocation.X, ScreenLocation.Y + LineOffsetY);
		float KeyWidth = 0.0f;
		float KeyHeight = 0.0f;
		Canvas->StrLen(Font, Message.KeyText, KeyWidth, KeyHeight);
		FCanvasTextItem KeyItem(TextPosition, FText::FromString(Message.KeyText), Font, Message.KeyColor);
		Canvas->DrawItem(KeyItem);
		// Canvas does not reliably expand tabs; use an explicit screen-space gap.
		constexpr float KeyValueGap = 24.0f;
		FCanvasTextItem ValueItem(TextPosition + FVector2D(KeyWidth + KeyValueGap, 0.0f),
			FText::FromString(Message.ValueText), Font, Message.ValueColor);
		Canvas->DrawItem(ValueItem);
		LineOffsetY += KeyHeight + LineSpacing;
	}
#endif
}

void UBaseAnimInstance::ClearDebugKeyValueMessages()
{
	if (DebugKeyValueDrawHandle.IsValid())
	{
		UDebugDrawService::Unregister(DebugKeyValueDrawHandle);
		DebugKeyValueDrawHandle.Reset();
	}
	DebugKeyValueMessages.Reset();
	DebugKeyValueFrame = MAX_uint64;
}

void UBaseAnimInstance::BeginDestroy()
{
	ClearDebugKeyValueMessages();
	Super::BeginDestroy();
}

void UBaseAnimInstance::NativeUninitializeAnimation()
{
	ClearDebugKeyValueMessages();
	Super::NativeUninitializeAnimation();
}

#pragma endregion DrawDebugMessages

void UBaseAnimInstance::UpdateEssentialData()
{
	//Sequence 0:	RotationMode
	if (AsBaseController != nullptr)
	{
		RotationMode = AsBaseController->CurrentRotationMode;
		bIsGamepadInput = AsBaseController->bIsGamepadInput;
	}

	if (MovementComponent != nullptr)
	{
		//Sequence 1:	bIsMoving	LastVelocityRotation
		const FVector Velocity = MovementComponent->Velocity;
		GroundSpeed = Velocity.Size2D();
		bIsMoving = GroundSpeed > 1.0f;
		if (bIsMoving)
		{
			LastVelocityRotation = Velocity.Rotation();
		}

		//Sequence 2:	bHasInput	LastInputRotation
		bHasInput = MovementComponent->GetAnalogInputModifier() > 0.0f;
		if (bHasInput)
		{
			LastInputRotation = MovementComponent->GetLastInputVector().Rotation();
		}
	}

	//Sequence 3:	bIsStrafing
	if (AsBaseController != nullptr)
	{
		bIsStrafing = RotationMode != ERotationMode::Rotating;
	}

	//Sequence 4:	AnimPlaySpeed
	const double SpeedRatio = UKismetMathLibrary::SafeDivide(
		GroundSpeed,
		GetCurveValue(SpeedCurveName));
	AnimPlaySpeed = static_cast<float>(
		FMath::Clamp(SpeedRatio, 0.8, 1.0)
		);
}

void UBaseAnimInstance::UpdateGroundGait()
{
	if (MovementComponent == nullptr)
	{
		return;
	}

	//速度与加速度反向时应该转身
	const FVector NormalizedVelocity =
		MovementComponent->Velocity.GetSafeNormal(0.0001f);
	const FVector NormalizedAcceleration =
		MovementComponent->GetCurrentAcceleration().GetSafeNormal(0.0001f);
	const bool bShouldTurn =
		FVector::DotProduct(NormalizedVelocity, NormalizedAcceleration) < -0.5f;

	if (MovementComponent->IsFalling())
	{
		GroundGait = EGroundGait::Idle;
		return;
	}
	
	//根据移动速度和推入系数判断是否可以切换到Run
	const bool bCanRun =
		bIsMoving
		&& MovementComponent->GetMaxSpeed() > 250.0f
		&& MovementComponent->GetAnalogInputModifier() > 0.6f;

	if (bCanRun)
	{
		GroundGait = bShouldTurn ? EGroundGait::Idle : EGroundGait::Run;
		return;
	}

	const bool bCanWalk =
		bIsMoving
		&& MovementComponent->GetMaxSpeed() > 50.0f
		&& MovementComponent->GetAnalogInputModifier() > 0.01f;

	GroundGait = bCanWalk && !bShouldTurn
		? EGroundGait::Walk
		: EGroundGait::Idle;
}

void UBaseAnimInstance::UpdateLeftFootUp()
{
	USkeletalMeshComponent* OwningComponent = GetOwningComponent();
	APawn* PawnOwner = TryGetPawnOwner();
	if (OwningComponent == nullptr || PawnOwner == nullptr)
	{
		bIsLeftFootUp = false;
		return;
	}

	static const FName LeftFootSocketName(TEXT("foot_l"));
	static const FName RightFootSocketName(TEXT("foot_r"));

	//局部左脚方向
	const FVector LeftFootDirection =
		OwningComponent
			->GetSocketTransform(LeftFootSocketName, RTS_Component)
			.GetLocation()
			.GetSafeNormal2D(0.0001f);
	//局部右脚方向
	const FVector RightFootDirection =
		OwningComponent
			->GetSocketTransform(RightFootSocketName, RTS_Component)
			.GetLocation()
			.GetSafeNormal2D(0.0001f);
	const FVector VelocityDirection =
		PawnOwner->GetVelocity().GetSafeNormal2D(0.0001f);
	//局部速度方向
	const FVector LocalVelocityDirection =
		OwningComponent->GetComponentRotation().UnrotateVector(VelocityDirection);

	const float LeftFootDot =
		FVector::DotProduct(LeftFootDirection, LocalVelocityDirection);
	const float RightFootDot =
		FVector::DotProduct(LocalVelocityDirection, RightFootDirection);

	bIsLeftFootUp = LeftFootDot > RightFootDot;
}

float UBaseAnimInstance::SetStopAnimStartTime(
	const float LowSpeedStartTime,
	const float MediumSpeedStartTime,
	const float HighSpeedStartTime) const
{
	if (GroundSpeed < 300.0f)
	{
		return LowSpeedStartTime;
	}

	if (GroundSpeed <= 400.0f)
	{
		return MediumSpeedStartTime;
	}

	return HighSpeedStartTime;
}


void UBaseAnimInstance::SmoothVelocityRotation(const float TargetInterpSpeed, const float ActorInterpSpeed)
{
	//PrimaryRotation = LastVelocityRotation
	PrimaryRotation = FMath::RInterpConstantTo(
		PrimaryRotation,
		LastVelocityRotation,
		DeltaTimeX,
		TargetInterpSpeed);

	if (AsBaseController == nullptr)
	{
		return;
	}

	//抵消：动画曲线RotateCurve带来的旋转偏移量
	const float RotateAlpha = GetCurveValue(RotateCurveName);
	FRotator TargetActorRotation = PrimaryRotation;
	TargetActorRotation.Yaw =
		PrimaryRotation.Yaw - RotateAlpha * StartAngle;

	const FRotator NewActorRotation = FMath::RInterpTo(
		AsBaseController->GetActorRotation(),
		TargetActorRotation,
		DeltaTimeX,
		ActorInterpSpeed);

	AsBaseController->SetActorRotation(NewActorRotation);
}

void UBaseAnimInstance::SmoothControlRotation(const float TargetInterpSpeed, const float ActorInterpSpeed)
{
	if (AsBaseController == nullptr)
	{
		return;
	}

	//PrimaryRotation = ControlRotation.Yaw
	const FRotator ControlRotation = AsBaseController->GetControlRotation();
	const FRotator TargetRotation(0.0f, ControlRotation.Yaw, 0.0f);
	PrimaryRotation = FMath::RInterpTo(
		PrimaryRotation,
		TargetRotation,
		DeltaTimeX,
		TargetInterpSpeed);

	//ActorRotation => PrimaryRotation
	const FRotator NewActorRotation = FMath::RInterpTo(
		AsBaseController->GetActorRotation(),
		PrimaryRotation,
		DeltaTimeX,
		ActorInterpSpeed);

	AsBaseController->SetActorRotation(NewActorRotation);
}

void UBaseAnimInstance::UpdateCurrentStateName(FName StateName)
{
	if (CurrentStateName != StateName)
	{
		CurrentStateName = StateName;
	}
}

// void UBaseAnimInstance::TurnInPlace_Rotating()
// {
// 	bIsShouldTurnInPlace = false;
// 	if (!IsValid(AsBaseController))
// 	{
// 		return;
// 	}
//
// 	//原地转身角度 = 目标朝向PrimaryRotation - 角色Rotation
// 	TurnInPlaceAngle = UKismetMathLibrary::NormalizedDeltaRotator(
// 		PrimaryRotation, AsBaseController->GetActorRotation()).Yaw;
// 	if (FMath::Abs(TurnInPlaceAngle) > 60.0)
// 	{
// 		bIsShouldTurnInPlace = true;
// 	}
// }

void UBaseAnimInstance::TurnInPlace_Rotating()
{
	bIsShouldTurnInPlace = false;

	if (!IsValid(AsBaseController) || GroundGait != EGroundGait::Idle)
	{
		return;
	}

	FRotator TargetRotation = PrimaryRotation;

	if (AsBaseController->bShouldAim)
	{
		if (const APlayerController* PlayerController =
			Cast<APlayerController>(AsBaseController->GetController()))
		{
			FVector ViewLocation;
			PlayerController->GetPlayerViewPoint(ViewLocation, TargetRotation);
		}
		else
		{
			TargetRotation = AsBaseController->GetControlRotation();
		}

		TargetRotation.Pitch = 0.0f;
		TargetRotation.Roll = 0.0f;
	}

	TurnInPlaceAngle = UKismetMathLibrary::NormalizedDeltaRotator(
		TargetRotation,
		AsBaseController->GetActorRotation()).Yaw;

	bIsShouldTurnInPlace = FMath::Abs(TurnInPlaceAngle) > 60.0;
}
