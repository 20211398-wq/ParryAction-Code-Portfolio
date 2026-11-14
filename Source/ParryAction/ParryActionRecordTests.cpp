#include "ParryActionSaveGame.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FParryActionRecordTest,
    "ParryAction.Records.BestTimeAndSerialization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FParryActionRecordTest::RunTest(const FString& Parameters)
{
    UParryActionSaveGame* Record = NewObject<UParryActionSaveGame>();
    if (!TestNotNull(TEXT("Create a fresh record"), Record))
    {
        return false;
    }
    TestEqual(TEXT("A fresh record has no victory"), Record->BestVictoryCentiseconds, -1);
    TestFalse(TEXT("A negative time is rejected"), Record->TryUpdateBestVictory(-1));
    TestEqual(TEXT("Invalid time does not change the record"), Record->BestVictoryCentiseconds, -1);
    TestTrue(TEXT("The first victory creates a record"), Record->TryUpdateBestVictory(12345));
    TestFalse(TEXT("A slower victory is not a record"), Record->TryUpdateBestVictory(14000));
    TestFalse(TEXT("An equal victory is not a new record"), Record->TryUpdateBestVictory(12345));
    TestEqual(TEXT("Slower or equal times preserve the best"), Record->BestVictoryCentiseconds, 12345);
    TestTrue(TEXT("A faster victory improves the record"), Record->TryUpdateBestVictory(12001));
    TestEqual(TEXT("The improved time is stored"), Record->BestVictoryCentiseconds, 12001);

    // Memory serialization exercises Unreal's save format without touching the player's slot.
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("Serialize the record"), UGameplayStatics::SaveGameToMemory(Record, Bytes)))
    {
        return false;
    }
    UParryActionSaveGame* Loaded = Cast<UParryActionSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if (!TestNotNull(TEXT("Deserialize the record"), Loaded))
    {
        return false;
    }
    TestEqual(TEXT("The improved time survives serialization"), Loaded->BestVictoryCentiseconds, 12001);
    TestFalse(TEXT("A slower time after loading still preserves the record"), Loaded->TryUpdateBestVictory(13000));
    TestEqual(TEXT("The loaded best remains unchanged"), Loaded->BestVictoryCentiseconds, 12001);
    return !HasAnyErrors();
}
#endif
