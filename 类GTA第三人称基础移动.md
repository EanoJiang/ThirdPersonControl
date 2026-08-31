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
