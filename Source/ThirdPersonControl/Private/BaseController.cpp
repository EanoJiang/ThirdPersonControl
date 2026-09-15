// Fill out your copyright notice in the Description page of Project Settings.


#include "BaseController.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

void ABaseController::InitialGaitSettings()
{
	FSGaitSettings WalkSettings;
	WalkSettings.MaxWalkSpeed = 165.0f;
	WalkSettings.MaxAcceleration = 500.0f;
	WalkSettings.BrakingDeceleration = 1500.0f;
	WalkSettings.BrakingFrictionFactor = 0.0f;
	WalkSettings.bUseSeparateBrakingFriction = false;
	WalkSettings.BrakingFriction = 0.0f;
	WalkSettings.GroundFriction = 8.0f;
	GaitSettings.Add(EGroundGait::Walk, WalkSettings);

	FSGaitSettings RunSettings;
	RunSettings.MaxWalkSpeed = 375.0f;
	RunSettings.MaxAcceleration = 500.0f;
	RunSettings.BrakingDeceleration = 1500.0f;
	RunSettings.BrakingFrictionFactor = 0.0f;
	RunSettings.bUseSeparateBrakingFriction = false;
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

// Called when the game starts or when spawned
void ABaseController::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ABaseController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ABaseController::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}


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

void ABaseController::SetMainAnimInstance()
{
	MainAnimInstance = GetMesh()->GetAnimInstance();
}

FVector2D ABaseController::FilterGamepadValue(const FVector2D& InputActionValue, float LowerThreshold) const
{
	return FVector2D(
		FMath::Abs(InputActionValue.X) > LowerThreshold ? InputActionValue.X : 0.0f,
		FMath::Abs(InputActionValue.Y) > LowerThreshold ? InputActionValue.Y : 0.0f);
}
