// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "BaseControllerData.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BaseController.generated.h"

class UAnimInstance;

UCLASS()
class THIRDPERSONCONTROL_API ABaseController : public ACharacter
{
	GENERATED_BODY()

public:
	void InitialGaitSettings();
	
	// Sets default values for this character's properties
	ABaseController();

	//步态
	UPROPERTY(BlueprintReadWrite, Category = "Gait")
	TMap<EGroundGait, FSGaitSettings> GaitSettings;
	UPROPERTY(BlueprintReadWrite, Category = "Gait")
	EGroundGait CurrentGroundGait;
	UPROPERTY(BlueprintReadWrite, Category = "Gait")
	EGroundGait PreviousGroundGait;
	//旋转模式
	UPROPERTY(BlueprintReadWrite, Category = "RotationMode")
	ERotationMode CurrentRotationMode;
	UPROPERTY(BlueprintReadWrite, Category = "RotationMode")
	ERotationMode PreviousRotationMode;

	//Anim实例
	UPROPERTY(BlueprintReadWrite, Category = "Anim")
	UAnimInstance* MainAnimInstance;

	//当前为手柄输入
	UPROPERTY(BlueprintReadWrite, Category = "Input")
	bool bIsGamepadInput = false;
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	//设置步态
	UFUNCTION(BlueprintCallable, Category = "Gait", meta = (HideSelfPin = "true"))
	void SetGroundGait(EGroundGait NewGroundGait);
	//设置旋转模式
	UFUNCTION(BlueprintCallable, Category = "RotationMode", meta = (HideSelfPin = "true"))
	void SetRotationMode(ERotationMode NewRotationMode);

	//建立Anim实例
	UFUNCTION(BlueprintCallable, Category = "Anim", meta = (HideSelfPin = "true"))
	void SetMainAnimInstance();

	//根据阈值过滤手柄输入的 X/Y 轴
	UFUNCTION(BlueprintPure, Category = "Input", meta = (HideSelfPin = "true"))
	FVector2D FilterGamepadValue(const FVector2D& InputActionValue, float LowerThreshold) const;

};
