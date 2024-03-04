// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/CustomMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Climber/ClimberCharacter.h"
#include "Climber/DebugHelper.h"

void UCustomMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
  Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

  /*
  TraceClimbableSurfaces();
  TraceFromEyeHeight(100.0f);
  */
}

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
    }
    else
    {
      Debug::Print(TEXT("Can NOT start climbing"));
    }
  }
  else
  {
    // Stop climbing
  }
}

bool UCustomMovementComponent::CanStartClimbing()
{
  if (IsFalling()) return false;
  if (!TraceClimbableSurfaces()) return false;
  if (!TraceFromEyeHeight(100.0f).bBlockingHit) return false;

  return true;
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
  ClimbableSurfacesTracedResults = DoCapsuleTraceMultiByObject(Start, End, true,true);

  return !ClimbableSurfacesTracedResults.IsEmpty();
}

FHitResult UCustomMovementComponent::TraceFromEyeHeight(float TraceDistance, float TraceStartOffset)
{
  const FVector ComponentLocation = UpdatedComponent->GetComponentLocation();
  const FVector EyeHeightOffset = UpdatedComponent->GetUpVector() * (CharacterOwner->BaseEyeHeight + TraceStartOffset);
  const FVector Start = ComponentLocation + EyeHeightOffset;
  const FVector End = Start + UpdatedComponent->GetForwardVector() * TraceDistance;
  return DoLineTraceSingleByObject(Start, End, true,true);
}

#pragma endregion
