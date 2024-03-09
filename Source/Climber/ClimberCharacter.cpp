// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClimberCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/CustomMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "MotionWarpingComponent.h"
#include "DebugHelper.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// AClimberCharacter

AClimberCharacter::AClimberCharacter(const FObjectInitializer& objectInitializer)
  : Super(objectInitializer.SetDefaultSubobjectClass<UCustomMovementComponent>(ACharacter::CharacterMovementComponentName))
{
  // Set size for collision capsule
  GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

  // Don't rotate when the controller rotates. Let that just affect the camera.
  bUseControllerRotationPitch = false;
  bUseControllerRotationYaw = false;
  bUseControllerRotationRoll = false;

  // Configure character movement
  CustomMovementComponent = Cast<UCustomMovementComponent>(GetCharacterMovement());
  GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
  GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

  // Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
  // instead of recompiling to adjust them
  GetCharacterMovement()->JumpZVelocity = 700.f;
  GetCharacterMovement()->AirControl = 0.35f;
  GetCharacterMovement()->MaxWalkSpeed = 500.f;
  GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
  GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
  GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

  // Create a camera boom (pulls in towards the player if there is a collision)
  CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
  CameraBoom->SetupAttachment(RootComponent);
  CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
  CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

  // Create a follow camera
  FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
  FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
  FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

  MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComp"));
}

void AClimberCharacter::BeginPlay()
{
  // Call the base class  
  Super::BeginPlay();

  //Add Input Mapping Context
  if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
  {
    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
    {
      Subsystem->AddMappingContext(DefaultMappingContext, 0);
    }
  }

  if (CustomMovementComponent)
  {
    CustomMovementComponent->OnEnterClimbStateDelegate.BindUObject(this, &ThisClass::OnPlayerEnterClimbState);
    CustomMovementComponent->OnExitClimbStateDelegate.BindUObject(this, &ThisClass::OnPlayerExitClimbState);
  }
}

//////////////////////////////////////////////////////////////////////////
// Input

void AClimberCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
  // Set up action bindings
  if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {

    // Jumping
    EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
    EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

    // Moving
    EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AClimberCharacter::Move);

    // Looking
    EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AClimberCharacter::Look);

    // Climbing
    EnhancedInputComponent->BindAction(ClimbAction, ETriggerEvent::Started, this, &AClimberCharacter::OnClimbActionStarted);
  }
  else
  {
    UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
  }
}

void AClimberCharacter::Move(const FInputActionValue& Value)
{
  if (!CustomMovementComponent) return;

  if (CustomMovementComponent->IsClimbing())
  {
    HandleClimbMovementInput(Value);
  }
  else
  {
    HandleGroundMovementInput(Value);
  }
}

void AClimberCharacter::HandleGroundMovementInput(const FInputActionValue& Value)
{
  // input is a Vector2D
  const FVector2D MovementVector = Value.Get<FVector2D>();

  if (Controller != nullptr)
  {
    // find out which way is forward
    const FRotator Rotation = Controller->GetControlRotation();
    const FRotator YawRotation(0, Rotation.Yaw, 0);

    // get forward vector
    const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

    // get right vector 
    const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

    // add movement 
    AddMovementInput(ForwardDirection, MovementVector.Y);
    AddMovementInput(RightDirection, MovementVector.X);
  }

}

void AClimberCharacter::HandleClimbMovementInput(const FInputActionValue& Value)
{
  // input is a Vector2D
  const FVector2D MovementVector = Value.Get<FVector2D>();

  const FVector ForwardDirection = FVector::CrossProduct(-CustomMovementComponent->GetClimbableSurfaceNormal(), GetActorRightVector());

  const FVector RightDirection = FVector::CrossProduct(-CustomMovementComponent->GetClimbableSurfaceNormal(), -GetActorUpVector());

  AddMovementInput(ForwardDirection, MovementVector.Y);
  AddMovementInput(RightDirection, MovementVector.X);
}

void AClimberCharacter::Look(const FInputActionValue& Value)
{
  // input is a Vector2D
  FVector2D LookAxisVector = Value.Get<FVector2D>();

  if (Controller != nullptr)
  {
    // add yaw and pitch input to controller
    AddControllerYawInput(LookAxisVector.X);
    AddControllerPitchInput(LookAxisVector.Y);
  }
}

void AClimberCharacter::OnClimbActionStarted(const FInputActionValue& Value)
{
  if (!CustomMovementComponent) return;

  if (!CustomMovementComponent->IsClimbing())
  {
    CustomMovementComponent->ToggleClimbing(true);
  }
  else
  {
    CustomMovementComponent->ToggleClimbing(false);
  }
}

void AClimberCharacter::OnPlayerEnterClimbState()
{
  Debug::Print(TEXT("enter"));
}

void AClimberCharacter::OnPlayerExitClimbState()
{
  Debug::Print(TEXT("exit"));
}