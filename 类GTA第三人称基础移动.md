```
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

	const FVector LeftFootDirection =
		OwningComponent->GetSocketLocation(LeftFootSocketName).GetSafeNormal2D(0.0001f);
	const FVector RightFootDirection =
		OwningComponent->GetSocketLocation(RightFootSocketName).GetSafeNormal2D(0.0001f);
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
```

# Part 01 自定义数据类型：枚举和结构体

```C++
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseControllerData.generated.h"

/*
 *移动方向枚举
 */
UENUM(BlueprintType)
enum class ECardinalDirection : uint8
{
	Backward	UMETA(DisplayName = "Backward"),
	Forward		UMETA(DisplayName = "Forward"),
	Left		UMETA(DisplayName = "Left"),
	Right		UMETA(DisplayName = "Right")
};

/*
 *步态枚举
 */
UENUM(BlueprintType)
enum class EGroundGait : uint8
{
	Idle	UMETA(DisplayName = "Idle"),
	Walk	UMETA(DisplayName = "Walk"),
	Run		UMETA(DisplayName = "Run")
};

/*
 *步态参数
 */
USTRUCT(BlueprintType)
struct FSGaitSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait")
	float StandingSpeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait")
	float Acceleration = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait")
	float BrakingDeceleration = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait")
	float BrakingFrictionFactor = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait")
	bool bUseSeparateBrakingFriction = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait")
	float BrakingFriction = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait")
	float GroundFriction = 0;
};

/*
 *旋转模式枚举
 */
UENUM(BlueprintType)
enum class ERotationMode : uint8
{
	Rotating	UMETA(DisplayName = "Rotating"),
	Strafing	UMETA(DisplayName = "Strafing")
};
```

# Part02 设置角色初始步态和旋转模式

## 用到的变量及默认值

```C++
	//步态
	UPROPERTY(BlueprintReadWrite, Category = Gait)
	TMap<EGroundGait, FSGaitSettings> GaitSettings;
	UPROPERTY(BlueprintReadWrite, Category = Gait)
	EGroundGait CurrentGroundGait;
	UPROPERTY(BlueprintReadWrite, Category = Gait)
	EGroundGait PreviousGroundGait;
	//旋转模式
	UPROPERTY(BlueprintReadWrite, Category = RotationMode)
	ERotationMode CurrentRotationMode;
	UPROPERTY(BlueprintReadWrite, Category = RotationMode)
	ERotationMode PreviousRotationMode;

	//Anim实例
	UPROPERTY(BlueprintReadWrite, Category = Anim)
	UAnimInstance* MainAnimInstance;
```

从RootMotion动画分析器可以看到，WalkLoop速度169.92，RunLoop速度386.19，因此可以设置一个接近的MaxWalkSpeed

![1788488074605](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260904171700920-473450955.png)

```C++
void ABaseController::InitialGaitSettings()
{
	FSGaitSettings WalkSettings;
	WalkSettings.MaxWalkSpeed = 175.0f;
	WalkSettings.MaxAcceleration = 350.0f;
	WalkSettings.BrakingDeceleration = 500.0f;
	WalkSettings.BrakingFrictionFactor = 1.0f;
	WalkSettings.bUseSeparateBrakingFriction = true;
	WalkSettings.BrakingFriction = 0.0f;
	WalkSettings.GroundFriction = 8.0f;
	GaitSettings.Add(EGroundGait::Walk, WalkSettings);

	FSGaitSettings RunSettings;
	RunSettings.MaxWalkSpeed = 375.0f;
	RunSettings.MaxAcceleration = 800.0f;
	RunSettings.BrakingDeceleration = 1200.0f;
	RunSettings.BrakingFrictionFactor = 1.0f;
	RunSettings.bUseSeparateBrakingFriction = true;
	RunSettings.BrakingFriction = 0.0f;
	RunSettings.GroundFriction = 8.0f;
	GaitSettings.Add(EGroundGait::Run, RunSettings);
}

// Sets default values
ABaseController::ABaseController()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	InitialGaitSettings();
}
```

## BeginPlay

![1788168092090](https://img2024.cnblogs.com/blog/3614909/202608/3614909-20260831184043829-782267344.png)

```C++
void ABaseController::SetMainAnimInstance()
{
	MainAnimInstance = GetMesh()->GetAnimInstance();
}
```

```C++
void ABaseController::SetGroundGait(EGroundGait NewGroundGait)
{
	if (CurrentGroundGait == NewGroundGait)
	{
		return;
	}

	//更新步态
	PreviousGroundGait = CurrentGroundGait;
	CurrentGroundGait = NewGroundGait;

	//更新CharacterMovement组件属性
	const FSGaitSettings* GaitSetting = GaitSettings.Find(CurrentGroundGait);
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (GaitSetting == nullptr || MovementComponent == nullptr)
	{
		return;
	}

	MovementComponent->MaxWalkSpeed = GaitSetting->MaxWalkSpeed;
	MovementComponent->MaxAcceleration = GaitSetting->MaxAcceleration;
	MovementComponent->BrakingDecelerationWalking = GaitSetting->BrakingDeceleration;
	MovementComponent->BrakingFrictionFactor = GaitSetting->BrakingFrictionFactor;
	MovementComponent->bUseSeparateBrakingFriction = GaitSetting->bUseSeparateBrakingFriction;
	MovementComponent->BrakingFriction = GaitSetting->BrakingFriction;
	MovementComponent->GroundFriction = GaitSetting->GroundFriction;
}
```

```C++
void ABaseController::SetRotationMode(ERotationMode NewRotationMode)
{
	if (CurrentRotationMode == NewRotationMode)
	{
		return;
	}

	//更新旋转模式
	PreviousRotationMode = CurrentRotationMode;
	CurrentRotationMode = NewRotationMode;
}
```

# Part03 更新步态数据+状态机

## 动画蓝图

![1788244861505](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260901174246166-191945141.png)

![1788244875571](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260901174246607-255727319.png)

```C++
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

	//计算动画蓝图需要的参数
	UFUNCTION(BlueprintCallable, Category = "UpdateData")
	void UpdateEssentialData();

	//更新地面步态
	UFUNCTION(BlueprintCallable, Category = "UpdateData")
	void UpdateGroundGait();
```

```C++
void UBaseAnimInstance::UpdateEssentialData()
{
	if (MovementComponent == nullptr)
	{
		GroundSpeed = 0.0f;
		bIsMoving = false;
		bHasInput = false;
		return;
	}

	//Sequence 0:
	const FVector Velocity = MovementComponent->Velocity;
	GroundSpeed = Velocity.Size2D();
	bIsMoving = GroundSpeed > 1.0f;
	if (bIsMoving)
	{
		LastVelocityRotation = Velocity.Rotation();
	}

	//Sequence 1:
	bHasInput = MovementComponent->GetAnalogInputModifier() > 0.0f;
	if (bHasInput)
	{
		LastInputRotation = MovementComponent->GetLastInputVector().Rotation();
	}

}
```

```C++
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
```

> UpdateGroundGait函数中：
> 切换为移动时转向的逻辑，用move-idle-move来实现

## 状态机

![1788259131256](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260904171701400-1292585422.png)

![](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260901174248261-1725324829.png)

灰色条件就是播完自动过渡

![1788259176495](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260904171701679-756065678.png)

较暗的条件是勾选了Disabled

![1788259160694](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260904171701872-499952415.png)

# Part04 移动停止+选择表

## 判断左脚是否抬起(也就是左脚是否在前)

```C++
	UPROPERTY(BlueprintReadWrite, Category = "CharacterState")
	bool bIsLeftFootUp = false;
```

```C++
	//更新移动方向上的领先脚
	UFUNCTION(BlueprintCallable, Category = "UpdateData")
	void UpdateLeftFootUp();
```

```C++
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
```

## 选择表

对所有Run_Stop动画序列添加距离曲线修改器

![1788252857284](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260901174248546-1756897093.png)

创建Uniform Indexable曲线压缩设置资产并分配到这些动画序列

> 如果打包后的游戏要运行：
>
> - Distance Match to Target
> - Advance Time by Distance Matching
> - 其他依赖 FAnimCurveBufferAccess 的距离匹配功能
>
> 那么相关动画的曲线必须使用 Uniform Indexable 压缩。否则编辑器里可能正常，打包后距离曲线查询可能失效，出现动画时间为 0、距离匹配不正确等问题。

![](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260901174249092-1830546549.png)

## Stop State

![1788487681799](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260904171702111-1078240827.png)

### AnimSetup_Stop

![1788493445480](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260904171702412-1584749210.png)

```C++
	//根据 GroundSpeed 计算Stop动画起始播放时间
	UFUNCTION(BlueprintPure, Category = "Animation", meta = (BlueprintThreadSafe, HideSelfPin = "true"))
	float SetStopAnimStartTime(
		float LowSpeedStartTime = 1.8f,
		float MediumSpeedStartTime = 1.5f,
		float HighSpeedStartTime = 0.0f) const;
```

```C++
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
```

### AnimUpdate_Stop

![1788490100942](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260904171702766-1199315228.png)

```C++
	UPROPERTY(BlueprintReadWrite, Category = "Curve")
	FName DistanceCurveName = FName("Distance");
```

## 效果

![1788490445874](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260904171703776-1882521978.gif)

# Part05 ALS的旋转框架

## 角色蓝图

InputGraph.StrafeAction

![1788512581664](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260904171704188-635167029.png)

## 动画蓝图

### Event BlueprintUpdateAnimation

![1788512535827](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260904171704374-160483754.png)

![1788512723146](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260904171704604-2077676571.png)

#### MovingRotation

![1788512728127](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260904171704818-1493213554.png)

```C++
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
```

```C++
	//Rotating模式下的旋转逻辑
	UFUNCTION(BlueprintCallable, Category = "CharacterRotation", meta = (HideSelfPin = "true"))
	void SmoothVelocityRotation(float TargetInterpSpeed, float ActorInterpSpeed);

	//Strafing模式下的旋转逻辑
	UFUNCTION(BlueprintCallable, Category = "CharacterRotation", meta = (HideSelfPin = "true"))
	void SmoothControlRotation(float TargetInterpSpeed, float ActorInterpSpeed);
```

```C++
void UBaseAnimInstance::SmoothVelocityRotation(const float TargetInterpSpeed, const float ActorInterpSpeed)
{
	//PrimaryRotation = LastVelocityRotation
	PrimaryRotation = FMath::RInterpTo(
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
```

#### NoMovingRotation

![1788512736385](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260904171705044-1016836592.png)

## 效果

![1788513376408](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260904171706022-1148433937.gif)

# Part06 Start资产选择
