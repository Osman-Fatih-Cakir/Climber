// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CustomMovementComponent.generated.h"

class UAnimMontage;
class UAnimInstance;

UENUM(BlueprintType)
namespace ECustomMovementMode
{
  enum Type
  {
    MOVE_Climb UMETA(DisplayName = "Climb Mode")
  };
}

UCLASS()
class CLIMBER_API UCustomMovementComponent : public UCharacterMovementComponent
{
  GENERATED_BODY()

protected:

#pragma region OverridenFunctions
  virtual void BeginPlay() override;
  virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
  virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
  virtual void PhysCustom(float deltaTime, int32 Iterations) override;
  virtual float GetMaxSpeed() const override;
  virtual float GetMaxAcceleration() const override;
#pragma endregion

private:

#pragma region ClimbTraces

  TArray<FHitResult> DoCapsuleTraceMultiByObject(const FVector& Start, const FVector& End, bool bShowDebugShape = false, bool bDrawPersistentShapes = false);
  FHitResult DoLineTraceSingleByObject(const FVector& Start, const FVector& End, bool bShowDebugShape = false, bool bDrawPersistentShapes = false);

#pragma endregion

#pragma region ClimbCore

  bool TraceClimbableSurfaces();
  FHitResult TraceFromEyeHeight(float TraceDistance, float TraceStartOffset = 0.0f);

  bool CanStartClimbing();

  void StartClimbing();
  void StopClimbing();

  void PhysClimb(float deltaTime, int32 Iterations);

  void ProcessClimbableSurfaceInfo();

  bool CheckShouldStopClimbing();

  FQuat GetClimbRotation(float deltaTime);

  void SnapMovementToClimbableSurfaces(float deltaTime);

  void PlayClimbMontage(UAnimMontage* MontageToPlay);

  UFUNCTION()
  void OnClimbMontageEndedd(UAnimMontage* Montage, bool bInterrupted);
#pragma endregion

#pragma region ClimbVariables

  TArray<FHitResult> ClimbableSurfacesTracedResults;

  FVector CurrentClimbableSurfaceLocation;

  FVector CurrentClimbableSurfaceNormal;

  UPROPERTY()
  UAnimInstance* OwningPlayerAnimInstance;
#pragma endregion

#pragma region ClimbBPVariables

  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement: Climbing", meta = (AllowPrivateAccess = "true"))
  TArray<TEnumAsByte<EObjectTypeQuery> > ClimbableSurfaceTraceTypes;

  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement: Climbing", meta = (AllowPrivateAccess = "true"))
  float ClimbCapsuleTraceRadius = 50.f;

  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement: Climbing", meta = (AllowPrivateAccess = "true"))
  float ClimbCapsuleTraceHalfHeight = 72.f;

  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement: Climbing", meta = (AllowPrivateAccess = "true"))
  float MaxBreakCLimbDeceleration = 400.0f;

  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement: Climbing", meta = (AllowPrivateAccess = "true"))
  float MaxClimbSpeed = 100.0f;

  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement: Climbing", meta = (AllowPrivateAccess = "true"))
  float MaxClimbAcceleration = 300.0f;

  UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement: Climbing", meta = (AllowPrivateAccess = "true"))
  UAnimMontage* IdleToClimbMontage;

#pragma endregion

public:
  void ToggleClimbing(bool bEnableClimb);
  bool IsClimbing() const;
  FORCEINLINE FVector GetClimbableSurfaceNormal() const { return CurrentClimbableSurfaceNormal; }
};
