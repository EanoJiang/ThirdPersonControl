// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimExecutionContext.h"
#include "Animation/AnimNodeReference.h"
#include "BaseControllerData.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "AnimNodes/AnimNode_SequenceEvaluator.h"
#include "BaseAnimInstance.generated.h"

class ABaseController;
class UCharacterMovementComponent;
class UCanvas;
class APlayerController;
class UChooserTable;

/**
 * 
 */
UCLASS()
class THIRDPERSONCONTROL_API UBaseAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UBaseAnimInstance();

#pragma region Variables
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

	//当前为手柄输入
	UPROPERTY(BlueprintReadWrite, Category = "Input")
	bool bIsGamepadInput = false;
	
	UPROPERTY(BlueprintReadWrite, Category = "CharacterState")
	FName CurrentStateName = FName("None");
	UPROPERTY(BlueprintReadWrite, Category = "AnimSequence")
	FString CurrentAnimSequenceName;
	
	//旋转模式下，是否原地转身
	UPROPERTY(BlueprintReadWrite, Category = "TurnInPlace")
	bool bShouldTurnInPlace_Rotating = false;
	//旋转模式下的原地转身角度
	UPROPERTY(BlueprintReadWrite, Category = "TurnInPlace")
	float TurnInPlaceAngle_Rotating = 0.0;
	
	//蒙太奇原地转身角度
	UPROPERTY(BlueprintReadWrite, Category = "TurnInPlace_Montage")
	float TurnInPlaceAngle_Montage = 0.0;

	//限制TurnInPlace的播放速度
	UPROPERTY(BlueprintReadWrite, Category = "TurnInPlace_Montage")
	float ScaledPlayRate_Montage = 1.0f;
	//限制TurnInPlace的旋转修正倍率
	UPROPERTY(BlueprintReadWrite, Category = "TurnInPlace_Montage")
	float TurnInPlaceAngleModifier_Montage;
	
	//Strafing模式下的人物方向
	UPROPERTY(BlueprintReadWrite, Category = "CharacterRotation|Strafing")
	ECardinalDirection VelocityCardinalDirection = ECardinalDirection::Forward;
	//Strafing模式下的Locomotion角度
	UPROPERTY(BlueprintReadWrite, Category = "CharacterRotation|Strafing")
	float VelocityLocomotionAngle = 90.0f;

	//快速切换到Cycle
	UPROPERTY(BlueprintReadWrite, Category = "CharacterState")
	bool bRotatingStartSwitchToStrafingCycle;
	//Start状态下的RotationMode
	UPROPERTY(BlueprintReadWrite, Category = "CharacterRotation|StartState")
	ERotationMode RotationMode_StartState = ERotationMode::Rotating;
	

#pragma endregion Variables

#pragma region Methods

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

	//Rotating模式下,计算原地转身State需要的数据
	UFUNCTION(BlueprintCallable, Category = "CharacterRotation", meta = (HideSelfPin = "true"))
	void UpdateStateData_TurnInPlace();

	//播放原地转身动画蒙太奇并应用旋转曲线控制实际角色转身角度
	UFUNCTION(BlueprintCallable, Category = "CharacterRotation", meta = (HideSelfPin = "true"))
	void TurnInPlace_Montage(TSoftObjectPtr<UChooserTable> CT_TurnInPlace_Montage);

	//根据移动角度选择方向，并扩大当前前后方向的死区。
	UFUNCTION(BlueprintPure, Category = "CharacterRotation|Strafing", meta = (HideSelfPin = "true"))
	ECardinalDirection SelectCardinalDirection(
		ECardinalDirection CurrentDirection,
		float CurrentAngle,
		float DeadZone,
		bool bUseCurrentDirection) const;


#pragma region 记录当前播放的动画序列名
	//记录当前播放的动画序列名
	UFUNCTION(BlueprintCallable, Category = "Animation|Sequence", meta = (BlueprintThreadSafe))
	void UpdateCurrentAnimSequenceName(FAnimNodeReference Node, UAnimSequenceBase* Montage);
	//重载
	void UpdateCurrentAnimSequenceName(UAnimSequenceBase* Montage)
	{
		UpdateCurrentAnimSequenceName(FAnimNodeReference(), Montage);
	}
#pragma endregion 
	
#pragma region 记录当前状态名
	void UpdateCurrentStateName(FName StateName);
	UFUNCTION(BlueprintCallable, Category = "Animation|State", meta = (BlueprintThreadSafe))
	void OnStateEntry_Idle(const FAnimUpdateContext& Context,const FAnimNodeReference& Node)
	{
		UpdateCurrentStateName(FName("Idle"));
	}
	UFUNCTION(BlueprintCallable, Category = "Animation|State", meta = (BlueprintThreadSafe))
	void OnStateEntry_Start(const FAnimUpdateContext& Context,const FAnimNodeReference& Node)
	{
		UpdateCurrentStateName(FName("Start"));
	}
	
	UFUNCTION(BlueprintCallable, Category = "Animation|State", meta = (BlueprintThreadSafe))
	void OnStateEntry_Cycle_Gait_Walk(const FAnimUpdateContext& Context,const FAnimNodeReference& Node)
	{
		UpdateCurrentStateName(FName("Cycle_Gait_Walk"));
	}
	UFUNCTION(BlueprintCallable, Category = "Animation|State", meta = (BlueprintThreadSafe))
	void OnStateEntry_Cycle_Gait_Run(const FAnimUpdateContext& Context,const FAnimNodeReference& Node)
	{
		UpdateCurrentStateName(FName("Cycle_Gait_Run"));
	}
	UFUNCTION(BlueprintCallable, Category = "Animation|State", meta = (BlueprintThreadSafe))
	void OnStateEntry_Cycle_GaitTransition_WalkToRun(const FAnimUpdateContext& Context,const FAnimNodeReference& Node)
	{
		UpdateCurrentStateName(FName("Cycle_GaitTransition_WalkToRun"));
	}
	UFUNCTION(BlueprintCallable, Category = "Animation|State", meta = (BlueprintThreadSafe))
	void OnStateEntry_Cycle_GaitTransition_RunToWalk(const FAnimUpdateContext& Context,const FAnimNodeReference& Node)
	{
		UpdateCurrentStateName(FName("Cycle_GaitTransition_RunToWalk"));
	}
	
	UFUNCTION(BlueprintCallable, Category = "Animation|State", meta = (BlueprintThreadSafe))
	void OnStateEntry_Stop(const FAnimUpdateContext& Context,const FAnimNodeReference& Node)
	{
		UpdateCurrentStateName(FName("Stop"));
	}	
	UFUNCTION(BlueprintCallable, Category = "Animation|State", meta = (BlueprintThreadSafe))
	void OnStateEntry_TurnInPlace_Rotating(const FAnimUpdateContext& Context,const FAnimNodeReference& Node)
	{
		UpdateCurrentStateName(FName("TurnInPlace_Rotating"));
	}
	UFUNCTION(BlueprintCallable, Category = "Animation|State", meta = (BlueprintThreadSafe))
	void OnStateEntry_TurnInPlace_Strafing(const FAnimUpdateContext& Context,const FAnimNodeReference& Node)
	{
		UpdateCurrentStateName(FName("TurnInPlace_Strafing"));
	}
	
#pragma endregion

#pragma endregion

#pragma region DrawDebugMessages
public:
#pragma region Methods
	virtual void NativeUninitializeAnimation() override;
	virtual void BeginDestroy() override;

	// 动画蓝图参数屏幕调试工具
	UFUNCTION(BlueprintCallable, Category = "DebugDraw")
	void DrawDebugKeyValueMessages(
		FName Key,
		const FString& Value,
		FLinearColor KeyColor = FLinearColor::White,
		FLinearColor ValueColor = FLinearColor::Red,
		FVector LocationOffset = FVector(0, -200, 100));

#pragma endregion Methods

private:
#pragma region Variables
	struct FDebugKeyValueMessage
	{
		FString KeyText;
		FString ValueText;
		FLinearColor KeyColor;
		FLinearColor ValueColor;
		FVector WorldLocation;
	};

	TArray<FDebugKeyValueMessage> DebugKeyValueMessages;
	uint64 DebugKeyValueFrame = MAX_uint64;
	FDelegateHandle DebugKeyValueDrawHandle;

#pragma endregion Variables

#pragma region Methods
	void DrawDebugKeyValueMessages(UCanvas* Canvas, APlayerController* PlayerController);
	void ClearDebugKeyValueMessages();
#pragma endregion Methods
#pragma endregion
};
