// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "BaseControllerData.h"
#include "BaseAnimInstance.generated.h"

class ABaseController;
class UCharacterMovementComponent;

/**
 * 
 */
UCLASS()
class THIRDPERSONCONTROL_API UBaseAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = "Reference")
	ABaseController* AsBaseController = nullptr;
	UPROPERTY(BlueprintReadWrite, Category = "Reference")
	UCharacterMovementComponent* MovementComponent = nullptr;
	UPROPERTY(BlueprintReadWrite, Category = "Reference")
	float DeltaTimeX = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "CharacterMovement")
	float GroundSpeed = 0.0f;
	UPROPERTY(BlueprintReadWrite, Category = "CharacterState")
	bool bIsMoving = false;
	UPROPERTY(BlueprintReadWrite, Category = "CharacterRotation")
	FRotator LastVelocityRotation = FRotator::ZeroRotator;
	UPROPERTY(BlueprintReadWrite, Category = "CharacterRotation")
	FRotator LastInputRotation = FRotator::ZeroRotator;
	UPROPERTY(BlueprintReadWrite, Category = "CharacterState")
	bool bHasInput = false;

	UPROPERTY(BlueprintReadWrite, Category = "CharacterMovement")
	EGroundGait GroundGait = EGroundGait::Idle;

	UPROPERTY(BlueprintReadWrite, Category = "CharacterState")
	bool bIsLeftFootUp = false;

	UPROPERTY(BlueprintReadWrite, Category = "Curve")
	FName DistanceCurveName = FName("Distance");

	UPROPERTY(BlueprintReadWrite, Category = "CharacterRotation")
	ERotationMode RotationMode = ERotationMode::Rotating;
	UPROPERTY(BlueprintReadWrite, Category = "CharacterState")
	bool bIsStrafing = false;

	UPROPERTY(BlueprintReadWrite, Category = "CharacterRotation")
	FRotator PrimaryRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadWrite, Category = "Curve")
	FName RotateCurveName = FName("RotateAlpha");
	//需要修正的总旋转角度
	UPROPERTY(BlueprintReadWrite, Category = "CharacterRotation")
	float StartAngle = 0.0f;

	//动画播放速率
	UPROPERTY(BlueprintReadWrite, Category = "AnimPlayRate")
	float AnimPlaySpeed = 1.0f;
	UPROPERTY(BlueprintReadWrite, Category = "Curve")
	FName SpeedCurveName = FName("MotionSpeed");

	//计算动画蓝图需要的参数
	UFUNCTION(BlueprintCallable, Category = "UpdateData")
	void UpdateEssentialData();

	//更新地面步态
	UFUNCTION(BlueprintCallable, Category = "UpdateData")
	void UpdateGroundGait();

	//更新移动方向上的领先脚
	UFUNCTION(BlueprintCallable, Category = "UpdateData")
	void UpdateLeftFootUp();

	//根据 GroundSpeed 计算Stop动画起始播放时间
	UFUNCTION(BlueprintPure, Category = "Animation", meta = (BlueprintThreadSafe, HideSelfPin = "true"))
	float SetStopAnimStartTime(
		float LowSpeedStartTime = 1.8f,
		float MediumSpeedStartTime = 1.5f,
		float HighSpeedStartTime = 0.0f) const;
	
	//Rotating模式下的旋转逻辑
	UFUNCTION(BlueprintCallable, Category = "CharacterRotation", meta = (HideSelfPin = "true"))
	void SmoothVelocityRotation(float TargetInterpSpeed, float ActorInterpSpeed);

	//Strafing模式下的旋转逻辑
	UFUNCTION(BlueprintCallable, Category = "CharacterRotation", meta = (HideSelfPin = "true"))
	void SmoothControlRotation(float TargetInterpSpeed, float ActorInterpSpeed);

private:
	FVector PreviousLeftFootLocation = FVector::ZeroVector;
	FVector PreviousRightFootLocation = FVector::ZeroVector;
	bool bHasPreviousFootSample = false;
};
