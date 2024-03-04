// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClimberGameMode.h"
#include "ClimberCharacter.h"
#include "UObject/ConstructorHelpers.h"

AClimberGameMode::AClimberGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
