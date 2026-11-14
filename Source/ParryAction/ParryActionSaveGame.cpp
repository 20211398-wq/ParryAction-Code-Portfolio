#include "ParryActionSaveGame.h"

bool UParryActionSaveGame::TryUpdateBestVictory(int32 NewCentiseconds)
{
    if (NewCentiseconds < 0 || (BestVictoryCentiseconds >= 0 && NewCentiseconds >= BestVictoryCentiseconds))
    {
        return false;
    }
    BestVictoryCentiseconds = NewCentiseconds;
    return true;
}
