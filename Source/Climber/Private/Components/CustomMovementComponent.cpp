// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/CustomMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Climber/ClimberCharacter.h"
#include "Climber/DebugHelper.h"
#include "Components/CapsuleComponent.h"

#pragma region OverridenFunctions

void UCustomMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
  Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

  /*
  TraceClimbableSurfaces();
  TraceFromEyeHeight(100.0f);
  */
}

void UCustomMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
  if (IsClimbing())
  {
    bOrientRotationToMovement = false;
    CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(48.0f);
  }

  if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == ECustomMovementMode::MOVE_Climb)
  {
    bOrientRotationToMovement = true;
    CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(96.0f);

    const FRotator DirtyRotation = UpdatedComponent->GetComponentRotation();
    const FRotator CleanStandRotation = FRotator(0.0f, DirtyRotation.Yaw, 0.0f);
    UpdatedComponent->SetRelativeRotation(CleanStandRotation);
    StopMovementImmediately();
  }

  Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
}

void UCustomMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
  if (IsClimbing())
  {
    PhysClimb(deltaTime, Iterations);
  }

  Super::PhysCustom(deltaTime, Iterations);
}

float UCustomMovementComponent::GetMaxSpeed() const
{
  if (IsClimbing())
  {
    return MaxClimbSpeed;
  }
  else
  {
    return Super::GetMaxSpeed();
  }
}

float UCustomMovementComponent::GetMaxAcceleration() const
{
  if (IsClimbing())
  {
    return MaxClimbAcceleration;
  }
  else
  {
    return Super::GetMaxAcceleration();
  }
}

#pragma endregion

#pragma region ClimbTraces

TArray<FHitResult> UCustomMovementComponent::DoCapsuleTraceMultiByObject(const FVector& Start, const FVector& End, bool bShowDebugShape, bool bDrawPersistentShapes)
{
  EDrawDebugTrace::Type DebugTraceType = EDrawDebugTrace::None;
  if (bShowDebugShape)
  {
    DebugTraceType = EDrawDebugTrace::ForOneFrame;
    if (bDrawPersistentShapes)
    {
      DebugTraceType = EDrawDebugTrace::Persistent;
    }
  }

  TArray<FHitResult> OutCapsuleTraceHitResults;
  UKismetSystemLibrary::CapsuleTraceMultiForObjects(
    this,
    Start,
    End,
    ClimbCapsuleTraceRadius,
    ClimbCapsuleTraceHalfHeight,
    ClimbableSurfaceTraceTypes,
    false,
    TArray<AActor*>(),
    DebugTraceType,
    OutCapsuleTraceHitResults,
    false
  );

  return OutCapsuleTraceHitResults;
}

FHitResult UCustomMovementComponent::DoLineTraceSingleByObject(const FVector& Start, const FVector& End, bool bShowDebugShape, bool bDrawPersistentShapes)
{
  EDrawDebugTrace::Type DebugTraceType = EDrawDebugTrace::None;
  if (bShowDebugShape)
  {
    DebugTraceType = EDrawDebugTrace::ForOneFrame;
    if (bDrawPersistentShapes)
    {
      DebugTraceType = EDrawDebugTrace::Persistent;
    }
  }

  FHitResult OutHit;
  UKismetSystemLibrary::LineTraceSingleForObjects(this,
    Start,
    End,
    ClimbableSurfaceTraceTypes,
    false,
    TArray<AActor*>(),
    DebugTraceType,
    OutHit,
    false);
  return OutHit;
}

#pragma endregion

#pragma region ClimbCore

void UCustomMovementComponent::ToggleClimbing(bool bEnableClimb)
{
  if (bEnableClimb)
  {
    if (CanStartClimbing())
    {
      // Enter climb state
      Debug::Print(TEXT("Can start climbing"));
      StartClimbing();
    }
    else
    {
      Debug::Print(TEXT("Can NOT start climbing"));
    }
  }
  else
  {
    // Stop climbing
    StopClimbing();
  }
}

bool UCustomMovementComponent::CanStartClimbing()
{
  if (IsFalling()) return false;
  if (!TraceClimbableSurfaces()) return false;
  if (!TraceFromEyeHeight(100.0f).bBlockingHit) return false;

  return true;
}

void UCustomMovementComponent::StartClimbing()
{
  SetMovementMode(MOVE_Custom, ECustomMovementMode::MOVE_Climb);
}

void UCustomMovementComponent::StopClimbing()
{
  SetMovementMode(MOVE_Falling);
}

void UCustomMovementComponent::PhysClimb(float deltaTime, int32 Iterations)
{
  if (deltaTime < MIN_TICK_TIME)
  {
    return;
  }

  /*Process all the climbable surfaces info*/
  TraceClimbableSurfaces();
  ProcessClimbableSurfaceInfo();

  /*Check if we should stop climbing*/
  if (CheckShouldStopClimbing())
  {
    StopClimbing();
  }

  RestorePreAdditiveRootMotionVelocity();

  if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
  {
    //Define the max climb speed and acceleration
    CalcVelocity(deltaTime, 0.0f, true, MaxBreakCLimbDeceleration);
  }

  ApplyRootMotionToVelocity(deltaTime);

  FVector OldLocation = UpdatedComponent->GetComponentLocation();
  const FVector Adjusted = Velocity * deltaTime;
  FHitResult Hit(1.f);

  //Handle climb rotation
  SafeMoveUpdatedComponent(Adjusted, GetClimbRotation(deltaTime), true, Hit);

  if (Hit.Time < 1.f)
  {
    //adjust and try again
    HandleImpact(Hit, deltaTime, Adjusted);
    SlideAlongSurface(Adjusted, (1.f - Hit.Time), Hit.Normal, Hit, true);
  }

  if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
  {
    Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / deltaTime;
  }

  /*Snap movement to climbable surfaces*/
  SnapMovementToClimbableSurfaces(deltaTime);
}

void UCustomMovementComponent::ProcessClimbableSurfaceInfo()
{
  CurrentClimbableSurfaceLocation = FVector::ZeroVector;
  CurrentClimbableSurfaceNormal = FVector::ZeroVector;

  if (ClimbableSurfacesTracedResults.IsEmpty()) return;

  for (const FHitResult& TracedHitResult : ClimbableSurfacesTracedResults)
  {
    CurrentClimbableSurfaceLocation += TracedHitResult.ImpactPoint;
    CurrentClimbableSurfaceNormal += TracedHitResult.ImpactNormal;
  }
  CurrentClimbableSurfaceLocation /= ClimbableSurfacesTracedResults.Num();
  CurrentClimbableSurfaceNormal = CurrentClimbableSurfaceNormal.GetSafeNormal();
}

bool UCustomMovementComponent::CheckShouldStopClimbing()
{
  if (ClimbableSurfacesTracedResults.IsEmpty()) return true;

  const float DotResult = FVector::DotProduct(CurrentClimbableSurfaceNormal, FVector::UpVector);
  const float DegreeDiff = FMath::RadiansToDegrees(FMath::Acos(DotResult));

  if (DegreeDiff <= 60.0f)
  {
    return true;
  }

  return false;
}

FQuat UCustomMovementComponent::GetClimbRotation(float deltaTime)
{
  const FQuat CurrentQuat = UpdatedComponent->GetComponentQuat();

  if (HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocity())
  {
    return CurrentQuat;
  }

  const FQuat TargetQuat = FRotationMatrix::MakeFromX(-CurrentClimbableSurfaceNormal).ToQuat();

  return FMath::QInterpTo(CurrentQuat, TargetQuat, deltaTime, 5.0f);;
}

void UCustomMovementComponent::SnapMovementToClimbableSurfaces(float deltaTime)
{
  const FVector ComponentForward = UpdatedComponent->GetForwardVector();
  const FVector ComponentLocation = UpdatedComponent->GetComponentLocation();

  const FVector ProjectedCharacterToSurface = (CurrentClimbableSurfaceLocation - ComponentLocation).ProjectOnTo(ComponentForward);
  const FVector SnapVector = -CurrentClimbableSurfaceNormal * ProjectedCharacterToSurface.Length();

  UpdatedComponent->MoveComponent(SnapVector * deltaTime * MaxClimbSpeed, UpdatedComponent->GetComponentQuat(), true);
}

bool UCustomMovementComponent::IsClimbing() const
{
  return MovementMode == MOVE_Custom && CustomMovementMode == ECustomMovementMode::MOVE_Climb;
}

// Trace for climbable surfaces, return true if there are valid surfaces
bool UCustomMovementComponent::TraceClimbableSurfaces()
{
  const FVector StartOffset = UpdatedComponent->GetForwardVector() * 30.0f;
  const FVector Start = UpdatedComponent->GetComponentLocation() + StartOffset;
  const FVector End = Start + UpdatedComponent->GetForwardVector();
  ClimbableSurfacesTracedResults = DoCapsuleTraceMultiByObject(Start, End, true);

  return !ClimbableSurfacesTracedResults.IsEmpty();
}

FHitResult UCustomMovementComponent::TraceFromEyeHeight(float TraceDistance, float TraceStartOffset)
{
  const FVector ComponentLocation = UpdatedComponent->GetComponentLocation();
  const FVector EyeHeightOffset = UpdatedComponent->GetUpVector() * (CharacterOwner->BaseEyeHeight + TraceStartOffset);
  const FVector Start = ComponentLocation + EyeHeightOffset;
  const FVector End = Start + UpdatedComponent->GetForwardVector() * TraceDistance;
  return DoLineTraceSingleByObject(Start, End);
}

