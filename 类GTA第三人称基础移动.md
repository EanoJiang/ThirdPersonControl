```
	//Rotating模式下判断是否需原地转身
	UFUNCTION(BlueprintCallable, Category = "CharacterRotation", meta = (HideSelfPin = "true"))
	void TurnInPlace_Rotating();
```

```
	//当前为手柄输入
	UPROPERTY(BlueprintReadWrite, Category = "Input")
	bool bIsGamepadInput = false;
```

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

> 适配手柄：摇杆推进自动切换步态

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

# Part04 移动停止+选择器表

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

## 动画序列添加Distance曲线修改器

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

## CT_StopState

![1788770297642](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260907163825704-662631360.png)

## Stop State

![1788487681799](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260904171702111-1078240827.png)

### AnimSetup_Stop

![1788493445480](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260904171702412-1584749210.png)

根据不同速度决定Stop动画的起始播放时间，而不是依赖同步组(参考Lyra，Stop和Locomotion并不是同一个同步组)

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
```

> 为什么计算用VelocityRotation的PrimaryRotation用`RInterpConstantTo`：
>
> 起步瞬间假设：
>
> ```
> ActorRotation       = 0°
> LastInputRotation   = 90°
> LastVelocityRotation= 0°
> ```
>
> `AnimSetup_Start` 先计算：
>
> ```
> PrimaryRotation = 90°
> StartAngle      = 90°
> ```
>
> 原来的 `RInterpTo(..., 800)` 随后让 `PrimaryRotation` 一帧跳向旧速度方向：
>
> ```
> PrimaryRotation：90° → 0°
> ```
>
> Start 开头 `RotateAlpha = 1`：
>
> ```
> TargetActorYaw
> = PrimaryRotation - RotateAlpha × StartAngle
> = 0° - 1 × 90°
> = -90°
> ```
>
> Actor 自己使用 `RInterpTo(..., 25)`。60 FPS 下：
>
> ```
> Alpha = 25 / 60 ≈ 0.4167
> ActorYaw = 0° + (-90°) × 0.4167
>          ≈ -37.5°
> ```
>
> 所以首帧角色被反向拉到大约 `-37.5°`。随后速度方向更新到新输入方向，`PrimaryRotation` 又跳回去，Actor 再往正确方向回拉，这就是“抽身”。
>
> 改成 [BaseAnimInstance.cpp (line 148)](D:/UE58Projects/ThirdPersonControl/Source/ThirdPersonControl/Private/BaseAnimInstance.cpp:148) 中的 `RInterpConstantTo` 后：
>
> ```
> PrimaryRotation：90° → 76.67°
> TargetActorYaw：76.67° - 90° = -13.33°
> Actor 首帧：约 -5.56°
> ```
>
> 反向误差被限制在很小的范围，不再产生明显抽身。
>
> 严格来说，`RInterpConstantTo` 也可能一帧到达目标，例如剩余角度小于 `800 × DeltaTime`。但它不会因为 `800 > 帧率` 就无条件吸附；这是两者最根本的区别。

#### NoMovingRotation

![1788512736385](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260904171705044-1016836592.png)

## 效果

![1788513376408](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260904171706022-1148433937.gif)

# Part06 Start资产选择

## 动画序列添加左右脚标记、RotateAlpha曲线修改器

> 一个前向起步，四个reface转向起步

![1788514721937](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260907151706998-480791995.png)

![1788514522495](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260907151707381-833881043.png)

> 确保RotateAlpha曲线从1->0
> 1代表有转向

![1788514984711](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260907151707921-1697485165.png)

## CT_StartState.Run.RunRotating

添加动画序列,范围区间参考[【UE】角色偏航角Yaw.Rotation分布规则](https://www.cnblogs.com/eanojiang/p/22845556)

![1788516152927](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260907151708456-81265498.png)

选择器表Debug

![1788768839725](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260907163342778-515847632.png)

## Start State

| 对比     | Sequence Player                 | Sequence Evaluator                                 |
| -------- | ------------------------------- | -------------------------------------------------- |
| 时间控制 | 节点内部自动累加时间            | 外部通过`Explicit Time`控制                      |
| 默认表现 | 动画会持续播放，可循环          | 时间不变就停在某一帧                               |
| 常见用途 | Idle、Walk、Run 等循环动画      | 距离匹配、落地、停止、按曲线或游戏逻辑控制动画进度 |
| 使用难度 | 简单，设置动画和 Play Rate 即可 | 需要自己更新时间或传入计算结果                     |

![1788748167289](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260907151708689-636314182.png)

### AnimSetup_Start

![1788748558731](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260907151708916-2049937875.png)

### AnimUpdate_Start

> 由于要适配手柄操作，随着摇杆推进会自动切换步态，因此需要把资产选择的ChooserTable放在Update里面

![1788748551013](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260907151709202-1081775779.png)

## 效果

![1788748754207](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260907151710182-1261351427.gif)

# Part07 曲线过渡和伪回转运动

> 出现问题：Start状态下开始转身前会有个抽身的动作
> 原因：动画的转向角度是死的，角色实际转向角度是活的，因此需要利用RotateAlpha曲线对StartAngle值进行缩放。曲线是从1开始衰减到0的，但是状态机过渡时会对曲线值也进行过渡操作，为了解决这个问题需要引入DeadBlending节点过滤掉不需要blend的动画曲线

## 引入DeadBlending节点

打印RotateAlpha曲线值可以发现，RotateAlpha是从0开始变化到1再衰减为0的，而不是直接从1开始衰减，这就会导致前半段出现抽身的动作

![1788749885721](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260907151710683-1137562493.png)

因此在动画蓝图中需要在最后加一个DeadBlending节点，用于屏蔽状态机过渡时的曲线值过渡

![1788750789389](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260907151710923-1133515554.png)

## 效果

![1788765420733](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260907151712472-518303694.gif)

# Part08 Start和Stop时的步态切换逻辑

![1788765673634](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260907163343340-1348927426.png)

> 对需要用到的Start WalkRotating资产进行和Start RunRotating同样的操作(添加左右脚标记、RotateAlpha曲线修改器)[
> ](#动画序列添加左右脚标记rotatealpha曲线修改器)![1788766399528](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260907163343751-1502314882.png)
> 操作详见[【动画序列添加左右脚标记、RotateAlpha曲线修改器】](#动画序列添加左右脚标记rotatealpha曲线修改器)

## CT_StartState的步态切换

![1788769200565](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260907163343975-896898324.png)

### CT_StartState.Walk.WalkRotating

![1788769338166](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260907163344212-1076999669.png)

## CT_StopState的步态切换

> 对需要用到的Stop WalkRotating资产进行和Stop RunRotating同样的操作(添加Distance修改器)[
> ](#选择表对所有run_stop动画序列添加距离曲线修改器)![1788769493488](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260907163344715-1960733068.png)
> 操作详见[【动画序列添加Distance曲线修改器】](#动画序列添加distance曲线修改器)

由于Stop状态时的EGroundGait已经更新为Idle，不能用EGroundGait来区分WalkStop和RunStop，因此用GroundSpeed区间来判定

![1788773310398](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260908120639566-1149826352.png)

## 效果

![1788774311595](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260908120640970-861448970.gif)

> 由于走路时只会有一只脚抬起，因此走路停步可以用判断哪只脚抬起来区分

# Part09 解决手柄输入不满时的滑步问题

## 角色蓝图

### 引入死区

InputGraph.IA_Move中根据阈值过滤手柄输入的 X/Y 轴

![1788839013796](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260908120641575-489220926.png)

```C++
	//根据阈值过滤手柄输入的 X/Y 轴
	UFUNCTION(BlueprintPure, Category = "Input", meta = (HideSelfPin = "true"))
	FVector2D FilterGamepadValue(const FVector2D& InputActionValue, float LowerThreshold) const;
```

```C++
FVector2D ABaseController::FilterGamepadValue(const FVector2D& InputActionValue, float LowerThreshold) const
{
	return FVector2D(
		FMath::Abs(InputActionValue.X) > LowerThreshold ? InputActionValue.X : 0.0f,
		FMath::Abs(InputActionValue.Y) > LowerThreshold ? InputActionValue.Y : 0.0f);
}
```

## IMC_Default中的手柄Move输入加上死区修改器

![1788785817420](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260908120642111-928204690.png)

## 动画蓝图

`MotionSpeed`曲线控制动画播放速率AnimPlayRate

```C++
	//动画播放速率
	UPROPERTY(BlueprintReadWrite, Category = "AnimPlayRate")
	float AnimPlaySpeed = 1.0f;
	UPROPERTY(BlueprintReadWrite, Category = "Curve")
	FName SpeedCurveName = FName("MotionSpeed");
```

```C++
void UBaseAnimInstance::UpdateEssentialData()
{
	//Sequence 0:	RotationMode
	if (AsBaseController != nullptr)
	{
		RotationMode = AsBaseController->CurrentRotationMode;
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
```

Start State的序列播放器PlayRate绑定AnimPlaySpeed参数传入，并且设置播放起点为0.1(防止起步动画开头顿一下)

![1788840327011](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260908120642322-208262477.png)

## 动画序列添加MotionSpeed曲线

![1788838687400](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260908120642549-83426614.png)

![1788838735056](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260908120642750-2070096319.png)

## 效果

![1788853907749](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260908155159234-874195882.gif)

# Part10 解决Walk转身起步时肩膀卡顿问题

> 复现条件：Walk转身起步时突然摇杆往同一侧推到底
> ![1788856059593](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260909193553622-2044749958.gif)

## 角色蓝图

区分一下当前是否是手柄输入

```C++
	//当前为手柄输入
	UPROPERTY(BlueprintReadWrite, Category = "Input")
	bool bIsGamepadInput = false;
```

InputGraph.AnyKey

![1788858563173](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260909193554299-677948223.png)

```C++
	//当前为手柄输入
	UPROPERTY(BlueprintReadWrite, Category = "Input")
	bool bIsGamepadInput = false;
```

```C++
void UBaseAnimInstance::UpdateEssentialData()
{
	//Sequence 0:	RotationMode
	if (AsBaseController != nullptr)
	{
		RotationMode = AsBaseController->CurrentRotationMode;
		bIsGamepadInput = AsBaseController->bIsGamepadInput;
	}
```

如果是手柄输入，只用90度的Start动画

![1788861722064](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260909193554812-1307989077.png)

![1788861749549](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260909193555094-1580546186.png)

## 效果

![1788866650314](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260909193556381-421295989.gif)

## 解决突然45度转向时的角色突然转向

> 原因：前向Start动画序列没有RotateAlpha信息

在用到的前向Start动画序列中的rotatealpha加一小段

![1788867712025](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260909193556990-744899144.png)

## 加上StrideWarping节点

> 优化滑步

![1788869441095](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260909193557271-41142523.png)

勾上InterpResult可以消除起步时的warping导致的掰腿卡顿现象

![1788870484179](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260909193557509-995018357.png)

# Part11 Cycle和同步组的设置

### 对需要用到的Cycle Walk和Cycle Run 添加左右脚标记、MotionSpeed曲线修改器

 ![1788943550417](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260909193557752-597386816.png)![1788943559403](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260909193557986-840022692.png)

> 添加左右脚标记详见：[动画序列添加左右脚标记、RotateAlpha曲线修改器](#动画序列添加左右脚标记rotatealpha曲线修改器)
> MotionSpeed曲线修改器详见：[动画序列添加MotionSpeed曲线](#动画序列添加motionspeed曲线)

### CT_CycleState

![1788943538739](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260909193558181-1800526485.png)

![1788943585454](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260909193558378-1923376724.png)

![1788943575618](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260909193558615-312852084.png)

## Cycle State

Cycle->Idle的跳转条件优先级设置低一级，防止出现直接跳转为Idle

![1789373150919](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260914211913305-270763410.png)

![1788943969782](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260909193558815-1852986595.png)

![1788943983642](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260909193559053-2080582572.png)

## 设置同步组

#### Cycle

![1788944051580](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260909193559278-1227846145.png)

#### Start

![1788944080340](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260909193559469-1415127680.png)

Start->Cycle的过渡时间改为0.5s，确保同步组正确匹配

![1789373079029](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260914211913567-1663219581.png)

## 效果

![1788948157586](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260909193601729-1912950169.gif)

# Part12 改善步态切换和回转运动效果

> 待办

# Part13 Rotating模式下的原地转身

> TurnInPlace

## 动画蓝图

UpdateCharacterRotation.NoMovingRotation

也就是不移动时的旋转逻辑中加入Rotating模式下的原地转身：

![1789530446453](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260916115603593-221198740.png)

```C++
	//旋转模式下的原地转身
	UPROPERTY(BlueprintReadWrite, Category = "CharacterState")
	bool bIsShouldTurnInPlace = false;
	/原地转身的角度
	UPROPERTY(BlueprintReadWrite, Category = "CharacterRotation")
	double TurnInPlaceAngle = 0.0;
```

```C++
	//Rotating模式下,计算原地转身State需要的数据
	UFUNCTION(BlueprintCallable, Category = "CharacterRotation", meta = (HideSelfPin = "true"))
	void UpdateStateData_TurnInPlace();
```

```C++
void UBaseAnimInstance::UpdateStateData_TurnInPlace()
{
	bShouldTurnInPlace_Rotating = false;

	if (!IsValid(AsBaseController) || GroundGait != EGroundGait::Idle)
	{
		return;
	}

	FRotator TargetRotation = (AsBaseController->bShouldAim) ? AsBaseController->GetControlRotation() : PrimaryRotation;


	TurnInPlaceAngle_Rotating = UKismetMathLibrary::NormalizedDeltaRotator(
		TargetRotation,
		AsBaseController->GetActorRotation()
		).Yaw;

	bShouldTurnInPlace_Rotating = FMath::Abs(TurnInPlaceAngle_Rotating) > 60.0;
}
```

## TurnInPlace State

![1789373026727](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260914211913835-1278417216.png)

> ***紫色：WantToTurnInPlace***![img](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260910210717227-262399486.png)

> ***把原先的Cycle->Start的跳转条件WantToStop改为连到Cycle->Stop，否则这里会出现bug：***
> 当角色进入CycleState，这时候松开移动输入键会进入StartState，而StartState相关联的AnimSetup_Start函数会更新PrimaryRotation，导致TurnInPlaceAngle = PrimaryRotation-Actor.Rotation > 60，触发ShouldTurnInPlace==true而进入TurnInPlaceState

## CT_TurnInPlaceState

![1789044625015](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260910210717443-1737518646.png)

# Part14 解决TurnInPlaceState播放动画RootMotion不起作用

原因：动画蓝图的默认设置中RootMotion是仅蒙太奇启用

![1789031623684](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260910210717770-460377420.png)

因此需要让原地转身动画在旋转期间回调动画通知对RootMotion进行设置：

Begin——设置RootMotion From Everything

![1789044913067](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260910210718047-1274499471.png)

End——恢复为RootMotion From Montages Only

![1789044919400](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260910210718302-1427823932.png)

Tick——如果有输入，就立刻恢复为RootMotion From Montages Only，从而打断原地转身动画

![1789044923580](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260910210718541-309706914.png)

在原地转身动画资产中添加该AnimNotifyState(最好是在root的z轴旋转不再变化时结束)：

![1789045157828](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260910210718917-249635152.png)

## 效果

![1789045568124](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260910210720553-1822242738.gif)

# Part15 键盘输入时转身Start对步态切换的限制

> 转身Start的时候切换步态的动作衔接问题：
> ![1789120558432](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260911190041282-1396493872.gif)

## Skeleton中添加DisableSwitchGroundGait曲线

在Skeleton中添加曲线DisableSwitchGroundGait，只有当DisableSwitchGroundGait==0的时候才能够切换步态

![1789123838351](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260911190043319-1860520479.png)

## 利用RotateAlpha曲线来赋值DisableSwitchGroundGait曲线

因为是StartState出现的步态切换动作衔接问题

因此在StartState中添加ModifyCurve节点，并且用转身起步动画资产中的RotateAlpha曲线来赋值DisableSwitchGroundGait曲线：

RotateAlpha有值的时候禁止切换步态，即DisableSwitchGroundGait=1

![1789123963256](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260911190043573-1067139253.png)

InputGraph.GaitAction：只有DisableSwitchGroundGait==0的时候才能切换步态

![1789124007972](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260911190043945-2048742590.png)

## 效果

![1789124866728](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260911190755199-1403727540.gif)

## 解决键盘切换步态为Walk时手柄无法自动切回Run

InputGraph.AnyKey：手柄输入时设置GroundGait为Run

![1789124821663](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260911190712928-1752887735.png)

或者在IMC_Default.IA_Walk中添加手柄切换步态的按键

# Part16 CycleState的步态过渡动画——垫步

## 对步态过渡动画添加左右脚标记、MotionSpeed曲线

![1789356046269](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260914211914517-1597806580.png)![1789356195455](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260914211914755-119931360.png)

> 添加左右脚标记详见：[动画序列添加左右脚标记、RotateAlpha曲线修改器](#动画序列添加左右脚标记rotatealpha曲线修改器)
> MotionSpeed曲线修改器详见：[动画序列添加MotionSpeed曲线](#动画序列添加motionspeed曲线)

## CT_GaitTransition

![1789356442569](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260914211914998-927084533.png)

## Cycle动画状态机重构

### CycleStateWithGaitTransition

![1789379868757](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260914211915235-2051747176.png)

![1789380458465](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260914211915462-1329946866.png)

跳转条件：

![](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260914211916464-380310387.png)

![](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260914211916823-361224968.png)

![](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260914211917115-62517200.png)

#### Cycle_Walk和Cycle_Run

![1789380379935](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260914211917530-103307838.png)

#### Cycle_WalkToRun和Cycle_RunToWalk

![1789380555569](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260914211917756-1313307296.png)

![1789380536097](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260914211917995-1089133558.png)

### 设置SequencePlayer的起始播放时间

> 步态过渡动画的前后帧片段较长，因此需要根据MotionSpeed曲线值的变化，设置他们State的SequencePlayer的起始播放时间

Cycle_WalkToRun

![1789376247245](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260914211918214-679646537.png)

![1789380652796](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260914211918447-1387370886.png)

Cycle_RunToWalk

![1789375789131](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260914211918689-497857012.png)

![1789380659778](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260914211918918-1811578440.png)

## 删去步态过渡动画的后续冗余帧片段

> 不能删去前序动画帧，因为状态之间的crossfade需要前序动画帧才能比较完美地融合

同样地，可以参考motionspeed曲线，也就是删去motionspeed已经稳定的帧片段

![1789386962677](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260914211919173-1706924754.png)

## 设置同步组

删去SequencePlayer起始播放时间前面的左右脚标记点，可以根据实际表现保留最近的那一个标记

(并注意这个时间点是哪只脚抬起而决定是否要在选择器表CT_CycleGaitTransition中修改资产的条件)

![1789387534151](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260914211919405-202991207.png)

将WalkToRun和RunToWalk的SequencePlayer的同步组都设置为Transition Leader

![1789370730920](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260914211919627-185394164.png)

## 限制步态切换时机

> 骨骼添加DisableSwitchGroundGait曲线详见：[Skeleton添加DisableSwitchGroundGait曲线](#skeleton添加disableswitchgroundgait曲线)

根据MotionSpeed曲线手动设置DisableSwitchGroundGait曲线(稍微比MotionSpeed曲线滞后一点)

WalkToRun

![1789377810536](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260914211919856-158316884.png)

RunToWalk

![1789377826646](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260914211920075-1421809475.png)

加一个跳转条件保证：**只有不在禁止步态切换期间动画**、**且步态还没切换到目标步态** 才会切过去，并且该条件的优先级需要低一档

![img](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260914211920501-1963646741.png)

## 效果

![1789392023671](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260914212027639-1339322873.gif)

# Part19 Strafing模式的原地转身

![1789444311841](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260915212225602-1941106566.png)

先全部取消rootmotion，添加TurnRotationAnimModifier修改器

![1789444404041](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260915212226010-944742918.png)

> TurnRotationAnimModifier修改器
> ![1789454928667](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260915212226500-186299519.png)

![1789455667005](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260915212226780-1114980545.png)

> 为了和Rotating下的TurnInPlace区分，将之前的加上后缀_Rotating

## CT_TurnInPlace_Montage

![1789529241219](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260916115603887-2011667181.png)

## 角色蓝图基类

```C++
	UPROPERTY(BlueprintReadWrite, Category = "Input")
	bool bShouldAim = false;
```

![1789118088241](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260911173943639-1449001572.png)

![1789118069930](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260911173944041-790920713.png)

## 动画蓝图

UpdateCharacterRotation.NoMovingRotation

![1789529980164](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260916115604104-520736951.png)

```C++
	//蒙太奇原地转身角度
	UPROPERTY(BlueprintReadWrite, Category = "TurnInPlace_Montage")
	float TurnInPlaceAngle_Montage = 0.0;

	//限制TurnInPlace的播放速度
	UPROPERTY(BlueprintReadWrite, Category = "TurnInPlace_Montage")
	float ScaledPlayRate_Montage = 1.0f;
	//限制TurnInPlace的旋转修正倍率
	UPROPERTY(BlueprintReadWrite, Category = "TurnInPlace_Montage")
	float TurnInPlaceAngleModifier_Montage;
```

```C++
	//播放原地转身动画蒙太奇并应用旋转曲线控制实际角色转身角度
	UFUNCTION(BlueprintCallable, Category = "CharacterRotation", meta = (HideSelfPin = "true"))
	void TurnInPlace_Montage(TSoftObjectPtr<UChooserTable> CT_TurnInPlace_Montage);
```

```C++
void UBaseAnimInstance::TurnInPlace_Montage(TSoftObjectPtr<UChooserTable> CT_TurnInPlace_Montage)
{
	static const FName EnableTurnInPlaceCurveName(TEXT("EnableTurnInPlace"));
	static const FName RotationAmountCurveName(TEXT("RotationAmount"));
	static const FName SlotName(TEXT("TurnInPlace_Montage"));

	// Sequence Then 0: Chooser 同时更新 ScaledPlayRate，再使用该值播放蒙太奇。
	if (GetCurveValue(EnableTurnInPlaceCurveName) == 1.0f && AsBaseController->bShouldAim)
	{
		TurnInPlaceAngle_Montage = UKismetMathLibrary::NormalizedDeltaRotator(
			AsBaseController->GetControlRotation(), AsBaseController->GetActorRotation()).Yaw;

		if (FMath::Abs(TurnInPlaceAngle_Montage) > 60.0f)
		{
			if (UChooserTable* ChooserTable = CT_TurnInPlace_Montage.LoadSynchronous())
			{
				UAnimSequenceBase* Animation = Cast<UAnimSequenceBase>(
					UChooserFunctionLibrary::EvaluateChooser(
						this, 
						ChooserTable, 
						UAnimSequenceBase::StaticClass()));
				if (IsValid(Animation) && !IsPlayingSlotAnimation(Animation, SlotName))
				{
					PlaySlotAnimationAsDynamicMontage(
						Animation, 
						SlotName,
						0.2f, 
						0.25f, 
						ScaledPlayRate_Montage, 
						1, 
						0.0f, 
						0.0f);

					const float AnimationAngle = FMath::Sign(TurnInPlaceAngle_Montage)
						* ( (FMath::Abs(TurnInPlaceAngle_Montage) < 130.0f ) ? 90.0f : 180.0f);
					//	(实际旋转角度 / 动画的旋转角度) * 播放速率 = 旋转倍率 * 播放速率 = 修正后的旋转倍率
					TurnInPlaceAngleModifier_Montage = (TurnInPlaceAngle_Montage / AnimationAngle) * ScaledPlayRate_Montage;
				}
			}
		}
	}

	// Sequence Then 1 独立执行，不受上面的动画触发条件影响。
	const float RotationAmount = GetCurveValue(RotationAmountCurveName);
	if (FMath::Abs(RotationAmount) > 0.0f)
	{
      //乘上DeltaTime用来适应不同帧率
		AsBaseController->AddActorWorldRotation(FRotator(
			0.0f, 
			RotationAmount * 45.0f * DeltaTimeX, 
			0.0f));
	}
}
```

回到CT_TurnInPlaceStrafing，返回ScaledPlayRate用来控制蒙太奇的播放速率

![1789530103756](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260916115604569-1821538497.png)

## 状态机

IdleState

* 让EnableTurnInPlace=1，也就是只有在Idle状态才会触发Strafing模式的原地转身
* 添加专属Slot
* 限制TurnInPlace的旋转修正倍率TurnInPlaceAngleModifier赋值给控制角色旋转的参数RotationAmount

![1789530153810](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260916115604804-935444258.png)

## 效果

> 均能正确转向

![1789478504755](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260915212229706-1497265334.gif)

## Aim时两种旋转模式共享原地转身逻辑

把这个Aim时播放原地转身蒙太奇的节点也放在Rotating模式下

![1789530651233](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260916115605006-904333131.png)

### 效果

![1789530959502](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260916115606984-1959098716.gif)


# Part999_1 根据当前抬起的脚选择Start和Stop动画

# Part999_2 移动时的身体倾斜

# Part999_3 不同方向起步的时机限制

![1789380171128](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260914211923701-2876125.gif)

![1789380167630](https://img2024.cnblogs.com/blog/3614909/202609/3614909-20260914211924463-1256544558.gif)
