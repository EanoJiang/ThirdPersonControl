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
	float MaxWalkSpeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait")
	float MaxAcceleration = 0;

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