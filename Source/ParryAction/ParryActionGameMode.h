// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ParryActionGameMode.generated.h"

/**
 *  Simple GameMode for a third person game
 */
UCLASS(abstract)
class AParryActionGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	
	/** Constructor */
	AParryActionGameMode();
};



