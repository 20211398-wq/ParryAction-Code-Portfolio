#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ParryActionSaveGame.generated.h"

/** Versioned local record for the current single-enemy challenge. */
UCLASS()
class PARRYACTION_API UParryActionSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    /** -1 means no victory has been recorded. */
    UPROPERTY(EditAnywhere, SaveGame, Category="Records")
    int32 BestVictoryCentiseconds = -1;

    /** Update only for a valid time that improves the saved best. */
    bool TryUpdateBestVictory(int32 NewCentiseconds);
};
