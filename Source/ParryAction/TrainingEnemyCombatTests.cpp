#include "TrainingEnemy.h"
#include "ParryActionCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainingEnemyAttackOutcomeTest,
    "ParryAction.Combat.EnemyAttackOutcome",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingEnemyAttackOutcomeTest::RunTest(const FString& Parameters)
{
    const auto Check = [this](const TCHAR* Description, bool bInRange, ETrainingAttackType Type,
        bool bParry, bool bDodge, ETrainingAttackOutcome Expected)
    {
        const ETrainingAttackOutcome Actual = ATrainingEnemy::ClassifyAttack(bInRange, Type, bParry, bDodge);
        TestEqual(Description, static_cast<int32>(Actual), static_cast<int32>(Expected));
    };
    Check(TEXT("Out of range takes precedence"), false, ETrainingAttackType::Parryable, true, true, ETrainingAttackOutcome::OutOfRange);
    Check(TEXT("Normal attack hits without defense"), true, ETrainingAttackType::Parryable, false, false, ETrainingAttackOutcome::PlayerHit);
    Check(TEXT("Normal attack can be parried"), true, ETrainingAttackType::Parryable, true, false, ETrainingAttackOutcome::Parried);
    Check(TEXT("Normal attack can be dodged"), true, ETrainingAttackType::Parryable, false, true, ETrainingAttackOutcome::Dodged);
    Check(TEXT("Parry wins when both defenses are active"), true, ETrainingAttackType::Parryable, true, true, ETrainingAttackOutcome::Parried);
    Check(TEXT("Danger attack ignores parry"), true, ETrainingAttackType::Unblockable, true, false, ETrainingAttackOutcome::PlayerHit);
    Check(TEXT("Danger attack can be dodged"), true, ETrainingAttackType::Unblockable, false, true, ETrainingAttackOutcome::Dodged);
    Check(TEXT("Dodge still protects against danger when parry is also active"), true, ETrainingAttackType::Unblockable, true, true, ETrainingAttackOutcome::Dodged);
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainingEnemyAttackSelectionTest,
    "ParryAction.Combat.AttackSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingEnemyAttackSelectionTest::RunTest(const FString& Parameters)
{
    const auto Check = [this](const TCHAR* Description, ETrainingAttackType Previous,
        int32 ConsecutiveCount, bool bPickUnblockable, ETrainingAttackType Expected)
    {
        const ETrainingAttackType Actual = ATrainingEnemy::ChooseNextAttack(Previous, ConsecutiveCount, bPickUnblockable);
        TestEqual(Description, static_cast<int32>(Actual), static_cast<int32>(Expected));
    };
    Check(TEXT("Normal may repeat once"), ETrainingAttackType::Parryable, 1, false, ETrainingAttackType::Parryable);
    Check(TEXT("Normal may switch to danger"), ETrainingAttackType::Parryable, 1, true, ETrainingAttackType::Unblockable);
    Check(TEXT("Two normal attacks force danger regardless of roll A"), ETrainingAttackType::Parryable, 2, false, ETrainingAttackType::Unblockable);
    Check(TEXT("Two normal attacks force danger regardless of roll B"), ETrainingAttackType::Parryable, 2, true, ETrainingAttackType::Unblockable);
    Check(TEXT("Danger may switch to normal"), ETrainingAttackType::Unblockable, 1, false, ETrainingAttackType::Parryable);
    Check(TEXT("Danger may repeat once"), ETrainingAttackType::Unblockable, 1, true, ETrainingAttackType::Unblockable);
    Check(TEXT("Two danger attacks force normal regardless of roll A"), ETrainingAttackType::Unblockable, 2, false, ETrainingAttackType::Parryable);
    Check(TEXT("Two danger attacks force normal regardless of roll B"), ETrainingAttackType::Unblockable, 2, true, ETrainingAttackType::Parryable);
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainingEnemyHealthTest,
    "ParryAction.Combat.EnemyHealthLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingEnemyHealthTest::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("Engine is available"), GEngine))
    {
        return false;
    }
    const FName WorldName = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("ParryActionDamageTestWorld"));
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
    if (!TestNotNull(TEXT("Create isolated test world"), TestWorld))
    {
        return false;
    }
    FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
    Context.SetCurrentWorld(TestWorld);
    TestWorld->InitializeActorsForPlay(FURL());
    TestWorld->BeginPlay();
    TestWorld->GetWorldSettings()->NotifyBeginPlay();

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ATrainingEnemy* Enemy = TestWorld->SpawnActor<ATrainingEnemy>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
    if (TestNotNull(TEXT("Spawn an enemy in the isolated world"), Enemy))
    {
        TestTrue(TEXT("Enemy has begun play"), Enemy->HasActorBegunPlay());
        TestNull(TEXT("Test world has no player controller or personal save"), TestWorld->GetFirstPlayerController());
        TestEqual(TEXT("Initial enemy health"), Enemy->GetCurrentHealth(), 160.0f);
        const auto Apply = [Enemy](float Damage)
        {
            return UGameplayStatics::ApplyDamage(Enemy, Damage, nullptr, nullptr, UDamageType::StaticClass());
        };
        TestEqual(TEXT("Zero damage has no effect"), Apply(0.0f), 0.0f);
        TestEqual(TEXT("Negative damage has no effect"), Apply(-10.0f), 0.0f);
        TestEqual(TEXT("Health remains full after invalid damage"), Enemy->GetCurrentHealth(), 160.0f);
        TestEqual(TEXT("A 20-damage hit is applied"), Apply(20.0f), 20.0f);
        TestEqual(TEXT("Health drops to 140"), Enemy->GetCurrentHealth(), 140.0f);
        TestFalse(TEXT("Enemy is still active"), Enemy->IsDefeated());
        TestEqual(TEXT("Lethal damage is clamped to remaining health"), Apply(1000.0f), 140.0f);
        TestEqual(TEXT("Defeated enemy health reaches zero"), Enemy->GetCurrentHealth(), 0.0f);
        TestTrue(TEXT("Enemy is defeated"), Enemy->IsDefeated());
        TestFalse(TEXT("Defeated enemy collision is disabled"), Enemy->GetActorEnableCollision());
        TestEqual(TEXT("Further damage is ignored"), Apply(20.0f), 0.0f);
        Enemy->Destroy();
    }

    TestWorld->EndPlay(EEndPlayReason::Quit);
    TestWorld->DestroyWorld(false);
    GEngine->DestroyWorldContext(TestWorld);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainingEnemyDangerComboTest,
    "ParryAction.Combat.DangerCombo",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// TimerManager advances once per engine frame; let the real frame counter advance.
class FDangerComboLatentCommand : public IAutomationLatentCommand
{
public:
    FDangerComboLatentCommand(FTrainingEnemyDangerComboTest* InTest, UWorld* InWorld,
        ATrainingEnemy* InEnemy, AParryActionCharacter* InPlayer, float InHealth, float InDamage)
        : Test(InTest), World(InWorld), Enemy(InEnemy), Player(InPlayer), InitialHealth(InHealth), HitDamage(InDamage),
          InitialEnemyX(InEnemy->GetActorLocation().X) {}

    virtual bool Update() override
    {
        World->Tick(LEVELTICK_All, 0.1f);
        if (++Ticks < 20) return false;
        Ticks = 0;
        if (Phase == 0)
        {
            Test->TestEqual(TEXT("Second strike applies the other half"), Player->GetCurrentHealth(), InitialHealth - 2.0f * HitDamage);
            Test->TestFalse(TEXT("Sequence finishes after the second strike"), Enemy->bWindingUp);
            Test->TestEqual(TEXT("No danger strike is shown after sequence"), Enemy->GetDangerStrikeNumber(), 0);
            Test->TestTrue(TEXT("Enemy lunges forward on safe ground"), Enemy->GetActorLocation().X > InitialEnemyX + 1.0f);

            // Place the player within distance but outside the locked forward arc.
            const FVector SidePosition = Enemy->GetActorLocation() + FVector(0.0f, 150.0f, 11.0f);
            Test->TestTrue(TEXT("Move player to the side"), Player->SetActorLocation(SidePosition, false, nullptr, ETeleportType::TeleportPhysics));
            HealthBeforeSideTest = Player->GetCurrentHealth();
            Enemy->AttackTarget = Player;
            Enemy->CurrentAttackType = ETrainingAttackType::Unblockable;
            Enemy->bWindingUp = true;
            Enemy->UnblockableStrikeIndex = 0;
            Enemy->LockedLungeDirection = FVector::ForwardVector;
            Enemy->bLungeDirectionLocked = true;
            Test->TestTrue(TEXT("Side target is within the distance limit before the first hit"), Enemy->CanReach(Player));
            Enemy->ResolveAttack();
            Test->TestEqual(TEXT("First strike misses a player to the side"), Player->GetCurrentHealth(), HealthBeforeSideTest);
            Test->TestEqual(TEXT("First side strike is classified out of the attack arc"), Enemy->GetRecentAttackOutcome(), static_cast<int32>(ETrainingAttackOutcome::OutOfRange));
            Phase = 1;
            return false;
        }
        if (Phase == 1)
        {
            Test->TestTrue(TEXT("Side target is still within distance after the second hit"), Enemy->CanReach(Player));
            Test->TestEqual(TEXT("Second strike also misses a player to the side"), Player->GetCurrentHealth(), HealthBeforeSideTest);
            Test->TestFalse(TEXT("Side sequence finishes after the second hit"), Enemy->bWindingUp);
            Test->TestTrue(TEXT("Second lunge kept the locked direction"), Enemy->LockedLungeDirection.Equals(FVector::ForwardVector));
            const FVector FrontPosition = Enemy->GetActorLocation() + FVector(150.0f, 0.0f, 11.0f);
            Test->TestTrue(TEXT("Move player back in front"), Player->SetActorLocation(FrontPosition, false, nullptr, ETeleportType::TeleportPhysics));
            Enemy->AttackTarget = Player;
            Enemy->CurrentAttackType = ETrainingAttackType::Unblockable;
            Enemy->bWindingUp = true;
            Enemy->UnblockableStrikeIndex = 0;
            Enemy->LockedLungeDirection = FVector::ForwardVector;
            Enemy->bLungeDirectionLocked = true;
            Enemy->ResolveAttack();
            HealthBeforeEnemyDefeat = Player->GetCurrentHealth();
            UGameplayStatics::ApplyDamage(Enemy, 10000.0f, nullptr, nullptr, UDamageType::StaticClass());
            Test->TestTrue(TEXT("Enemy was defeated during the sequence"), Enemy->IsDefeated());
            Phase = 2;
            return false;
        }
        Test->TestEqual(TEXT("Defeated enemy cannot deliver a pending second hit"), Player->GetCurrentHealth(), HealthBeforeEnemyDefeat);
        Enemy->Destroy();
        Player->Destroy();
        World->EndPlay(EEndPlayReason::Quit);
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
        return true;
    }
private:
    FTrainingEnemyDangerComboTest* Test;
    UWorld* World;
    ATrainingEnemy* Enemy;
    AParryActionCharacter* Player;
    float InitialHealth;
    float HitDamage;
    float InitialEnemyX;
    float HealthBeforeSideTest = 0.0f;
    float HealthBeforeEnemyDefeat = 0.0f;
    int32 Ticks = 0;
    int32 Phase = 0;
};

bool FTrainingEnemyDangerComboTest::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("Engine is available"), GEngine)) return false;
    UClass* PlayerClass = LoadClass<AParryActionCharacter>(nullptr,
        TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
    UClass* EnemyClass = LoadClass<ATrainingEnemy>(nullptr,
        TEXT("/Game/ThirdPerson/Blueprints/BP_TrainingEnemy.BP_TrainingEnemy_C"));
    if (!TestNotNull(TEXT("Load player Blueprint"), PlayerClass) || !TestNotNull(TEXT("Load enemy Blueprint"), EnemyClass)) return false;

    const FName WorldName = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("ParryActionComboTestWorld"));
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
    if (!TestNotNull(TEXT("Create isolated test world"), TestWorld)) return false;
    FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
    Context.SetCurrentWorld(TestWorld);
    UGameInstance* TestGameInstance = NewObject<UGameInstance>(GEngine);
    TestWorld->SetGameInstance(TestGameInstance);
    TestWorld->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
    TestTrue(TEXT("Create game mode for pawn damage"), TestWorld->SetGameMode(FURL()));
    TestWorld->InitializeActorsForPlay(FURL());
    TestWorld->BeginPlay();
    TestWorld->GetWorldSettings()->NotifyBeginPlay();

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* Floor = TestWorld->SpawnActor<AActor>(FVector(0, 0, -50), FRotator::ZeroRotator, Params);
    UStaticMesh* FloorMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Floor && FloorMesh)
    {
        UStaticMeshComponent* FloorComponent = NewObject<UStaticMeshComponent>(Floor);
        FloorComponent->SetStaticMesh(FloorMesh);
        FloorComponent->SetCollisionProfileName(TEXT("BlockAll"));
        Floor->SetRootComponent(FloorComponent);
        FloorComponent->SetWorldScale3D(FVector(20, 20, 1));
        FloorComponent->RegisterComponent();
        Floor->SetActorLocation(FVector(0, 0, -50));
    }
    AParryActionCharacter* Player = TestWorld->SpawnActor<AParryActionCharacter>(PlayerClass, FVector(150, 0, 96), FRotator::ZeroRotator, Params);
    ATrainingEnemy* Enemy = TestWorld->SpawnActor<ATrainingEnemy>(EnemyClass, FVector(0, 0, 85), FRotator::ZeroRotator, Params);
    if (TestNotNull(TEXT("Create a test floor"), Floor) && TestNotNull(TEXT("Load floor mesh"), FloorMesh)
        && TestNotNull(TEXT("Spawn player"), Player) && TestNotNull(TEXT("Spawn enemy"), Enemy))
    {
        TestNull(TEXT("No player controller or personal save"), TestWorld->GetFirstPlayerController());
        Player->GetCharacterMovement()->DisableMovement();
        const float InitialHealth = Player->GetCurrentHealth();
        const float HitDamage = FMath::Max(0.0f, Enemy->UnblockableDamage * 0.5f);
        Enemy->AttackTarget = Player;
        Enemy->CurrentAttackType = ETrainingAttackType::Unblockable;
        Enemy->bWindingUp = true;
        Enemy->UnblockableStrikeIndex = 0;
        Enemy->LockedLungeDirection = FVector::ForwardVector;
        Enemy->bLungeDirectionLocked = true;
        TestEqual(TEXT("First danger strike is announced"), Enemy->GetDangerStrikeNumber(), 1);
        Enemy->ResolveAttack();
        TestEqual(TEXT("Second danger strike is announced"), Enemy->GetDangerStrikeNumber(), 2);
        TestEqual(TEXT("First strike applies half danger damage"), Player->GetCurrentHealth(), InitialHealth - HitDamage);
        TestTrue(TEXT("Enemy remains committed to the second strike"), Enemy->bWindingUp && Enemy->UnblockableStrikeIndex == 1);
        ADD_LATENT_AUTOMATION_COMMAND(FDangerComboLatentCommand(this, TestWorld, Enemy, Player, InitialHealth, HitDamage));
        return true;
    }
    TestWorld->EndPlay(EEndPlayReason::Quit);
    TestWorld->DestroyWorld(false);
    GEngine->DestroyWorldContext(TestWorld);
    return !HasAnyErrors();
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStrongParryChallengeTest,
    "ParryAction.Combat.StrongParryChallenge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStrongParryChallengeTest::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("Engine available"), GEngine)) return false;
    UClass* PlayerClass = LoadClass<AParryActionCharacter>(nullptr,
        TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
    if (!TestNotNull(TEXT("Player Blueprint"), PlayerClass)) return false;
    const FName Name = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("StrongParryTestWorld"));
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, Name, GetTransientPackage());
    if (!TestNotNull(TEXT("World"), World)) return false;
    FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
    Context.SetCurrentWorld(World);
    World->SetGameInstance(NewObject<UGameInstance>(GEngine));
    World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
    TestTrue(TEXT("Game mode"), World->SetGameMode(FURL()));
    World->InitializeActorsForPlay(FURL());
    World->BeginPlay();
    World->GetWorldSettings()->NotifyBeginPlay();
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AParryActionCharacter* Player = World->SpawnActor<AParryActionCharacter>(PlayerClass, FVector(150, 0, 0), FRotator::ZeroRotator, Params);
    ATrainingEnemy* Enemy = World->SpawnActor<ATrainingEnemy>(ATrainingEnemy::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (TestNotNull(TEXT("Player spawned"), Player) && TestNotNull(TEXT("Enemy spawned"), Enemy))
    {
        Player->GetCharacterMovement()->DisableMovement();
        Enemy->ParrySuccessSoundAsset.Reset();
        Enemy->ParrySuccessEffectAsset.Reset();
        TestNull(TEXT("No controller or personal save before test"), World->GetFirstPlayerController());
        UClass* ControllerClass = LoadClass<APlayerController>(nullptr,
            TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"));
        APlayerController* TestController = ControllerClass
            ? World->SpawnActor<APlayerController>(ControllerClass, FVector::ZeroVector, FRotator::ZeroRotator, Params) : nullptr;
        TestNotNull(TEXT("Controller Blueprint"), TestController);
        if (TestController)
        {
            TestFalse(TEXT("Controller is not a local player and cannot write a personal record"), TestController->IsLocalPlayerController());
            TestController->Possess(Player);
            TestTrue(TEXT("Controller possesses test player"), TestController->GetPawn() == Player);
        }
        const auto StartFirstStrike = [Player, Enemy]()
        {
            Enemy->AttackTarget = Player;
            Enemy->CurrentAttackType = ETrainingAttackType::Unblockable;
            Enemy->bWindingUp = true;
            Enemy->UnblockableStrikeIndex = 0;
            Enemy->bLungeDirectionLocked = false;
            Player->StartParry();
            Enemy->ResolveAttack();
        };
        StartFirstStrike();
        TestTrue(TEXT("Parrying first strike starts challenge"), Enemy->IsDangerChallengeFor(Player));
        TestTrue(TEXT("Movement and attacks locked"), Player->IsDangerChallengeLocked());
        TestFalse(TEXT("Second strike timer is not running"), World->GetTimerManager().IsTimerActive(Enemy->WindupTimer));
        TestTrue(TEXT("Challenge timeout timer is active"), World->GetTimerManager().IsTimerActive(Enemy->DangerChallengeTimer));
        const FString Sequence = Enemy->DangerChallengeSequence;
        TestEqual(TEXT("Generated sequence has four keys"), Sequence.Len(), 4);
        for (TCHAR Key : Sequence)
        {
            if (!TestController) break;
            FInputKeyEventArgs Event;
            Event.Key = Key == TEXT('W') ? EKeys::W : Key == TEXT('A') ? EKeys::A : Key == TEXT('S') ? EKeys::S : EKeys::D;
            Event.Event = IE_Pressed;
            TestTrue(TEXT("Controller consumes challenge key"), TestController->InputKey(Event));
        }
        TestFalse(TEXT("Success closes challenge"), Enemy->IsDangerChallengeActive());
        TestFalse(TEXT("Success restores player controls"), Player->IsDangerChallengeLocked());
        TestTrue(TEXT("Success stuns enemy"), Enemy->IsStunned());
        TestEqual(TEXT("Success deals no damage"), Player->GetCurrentHealth(), 100.0f);
        TestEqual(TEXT("Success counts one parry"), Player->GetSuccessfulParryCount(), 1);
        TestEqual(TEXT("Strong stun uses the longer timer"), World->GetTimerManager().GetTimerRate(Enemy->StunTimer), Enemy->DangerParryStunSeconds);
        World->GetTimerManager().ClearTimer(Enemy->StunTimer);
        Enemy->EndParryStun();
        StartFirstStrike();
        TestTrue(TEXT("A new danger parry starts another challenge"), Enemy->IsDangerChallengeActive());
        if (Enemy->DangerChallengeSequence.Len() == 4)
        {
            const TCHAR Wrong = Enemy->DangerChallengeSequence[0] == TEXT('W') ? TEXT('A') : TEXT('W');
            Enemy->SubmitDangerChallengeKey(Wrong);
            TestFalse(TEXT("Wrong input closes challenge"), Enemy->IsDangerChallengeActive());
            TestFalse(TEXT("Wrong input restores controls"), Player->IsDangerChallengeLocked());
            TestEqual(TEXT("Wrong input applies double total damage once"), Player->GetCurrentHealth(), 40.0f);
            Enemy->SubmitDangerChallengeKey(Wrong);
            TestEqual(TEXT("Further inputs cannot repeat failure damage"), Player->GetCurrentHealth(), 40.0f);
            TestFalse(TEXT("No second strike after failure"), World->GetTimerManager().IsTimerActive(Enemy->WindupTimer));
        }
        StartFirstStrike();
        TestTrue(TEXT("Timeout challenge starts"), Enemy->IsDangerChallengeActive());
        Enemy->DangerChallengeTimeout();
        TestTrue(TEXT("Timeout can defeat player"), Player->IsDefeated());
        TestFalse(TEXT("Timeout closes challenge"), Enemy->IsDangerChallengeActive());
        TestFalse(TEXT("Timeout releases challenge lock"), Player->IsDangerChallengeLocked());
        if (TestController) TestController->Destroy();
        Enemy->Destroy();
        Player->Destroy();
    }
    World->EndPlay(EEndPlayReason::Quit);
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return !HasAnyErrors();
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStrongParryTimerTest,
    "ParryAction.Combat.StrongParryTimeout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

class FStrongParryTimeoutLatentCommand : public IAutomationLatentCommand
{
public:
    FStrongParryTimeoutLatentCommand(FStrongParryTimerTest* InTest, UWorld* InWorld,
        ATrainingEnemy* InEnemy, AParryActionCharacter* InPlayer)
        : Test(InTest), World(InWorld), Enemy(InEnemy), Player(InPlayer) {}
    virtual bool Update() override
    {
        World->Tick(LEVELTICK_All, 0.1f);
        ++Frames;
        if (Frames < 50) return false;
        Test->TestFalse(TEXT("Real timer closes challenge"), Enemy->IsDangerChallengeActive());
        Test->TestFalse(TEXT("Timeout releases player"), Player->IsDangerChallengeLocked());
        Test->TestEqual(TEXT("Timeout applies 60 damage once"), Player->GetCurrentHealth(), 40.0f);
        Enemy->Destroy();
        Player->Destroy();
        World->EndPlay(EEndPlayReason::Quit);
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
        return true;
    }
private:
    FStrongParryTimerTest* Test;
    UWorld* World;
    ATrainingEnemy* Enemy;
    AParryActionCharacter* Player;
    int32 Frames = 0;
};

bool FStrongParryTimerTest::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("Engine available"), GEngine)) return false;
    UClass* PlayerClass = LoadClass<AParryActionCharacter>(nullptr,
        TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
    if (!TestNotNull(TEXT("Player Blueprint"), PlayerClass)) return false;
    const FName Name = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("StrongParryTimerWorld"));
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, Name, GetTransientPackage());
    if (!TestNotNull(TEXT("World"), World)) return false;
    FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
    Context.SetCurrentWorld(World);
    World->SetGameInstance(NewObject<UGameInstance>(GEngine));
    World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
    TestTrue(TEXT("Game mode"), World->SetGameMode(FURL()));
    World->InitializeActorsForPlay(FURL());
    World->BeginPlay();
    World->GetWorldSettings()->NotifyBeginPlay();
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AParryActionCharacter* Player = World->SpawnActor<AParryActionCharacter>(PlayerClass, FVector(150, 0, 0), FRotator::ZeroRotator, Params);
    ATrainingEnemy* Enemy = World->SpawnActor<ATrainingEnemy>(ATrainingEnemy::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (TestNotNull(TEXT("Player"), Player) && TestNotNull(TEXT("Enemy"), Enemy))
    {
        TestNull(TEXT("No player controller or personal save"), World->GetFirstPlayerController());
        Player->GetCharacterMovement()->DisableMovement();
        Enemy->BeginDangerChallenge(Player);
        TestTrue(TEXT("Challenge begins"), Enemy->IsDangerChallengeActive());
        ADD_LATENT_AUTOMATION_COMMAND(FStrongParryTimeoutLatentCommand(this, World, Enemy, Player));
        return true;
    }
    World->EndPlay(EEndPlayReason::Quit);
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return !HasAnyErrors();
}

#endif
