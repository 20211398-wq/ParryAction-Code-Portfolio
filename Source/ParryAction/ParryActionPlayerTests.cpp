#include "ParryActionCharacter.h"
#include "TrainingEnemy.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FParryActionDefeatTimeTest,
    "ParryAction.Records.DefeatTime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FParryActionDefeatTimeTest::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("Engine is available"), GEngine)) return false;
    UClass* PlayerClass = LoadClass<AParryActionCharacter>(nullptr,
        TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
    if (!TestNotNull(TEXT("Load the player Blueprint"), PlayerClass)) return false;

    const FName WorldName = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("ParryActionDefeatTestWorld"));
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
    if (!TestNotNull(TEXT("Create isolated test world"), TestWorld)) return false;
    FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
    Context.SetCurrentWorld(TestWorld);
    // Pawns only accept damage in a world with an authoritative game mode.
    UGameInstance* TestGameInstance = NewObject<UGameInstance>(GEngine);
    TestWorld->SetGameInstance(TestGameInstance);
    TestWorld->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
    TestTrue(TEXT("Create a game mode for pawn damage"), TestWorld->SetGameMode(FURL()));
    TestWorld->InitializeActorsForPlay(FURL());
    TestWorld->BeginPlay();
    TestWorld->GetWorldSettings()->NotifyBeginPlay();

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AParryActionCharacter* Player = TestWorld->SpawnActor<AParryActionCharacter>(PlayerClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
    if (TestNotNull(TEXT("Spawn player in isolated world"), Player))
    {
        TestNull(TEXT("No controller or personal save is used"), TestWorld->GetFirstPlayerController());
        TestWorld->Tick(LEVELTICK_All, 0.5f);
        const float TimeBeforeDefeat = Player->GetCombatElapsedSeconds();
        TestTrue(TEXT("Combat time advances during play"), TimeBeforeDefeat > 0.0f);
        UGameplayStatics::ApplyDamage(Player, 10000.0f, nullptr, nullptr, UDamageType::StaticClass());
        TestTrue(TEXT("The player is defeated"), Player->IsDefeated());
        const float DefeatTime = Player->GetCombatElapsedSeconds();
        TestEqual(TEXT("Defeat is recorded immediately"), DefeatTime, TimeBeforeDefeat);
        const float WorldTimeAtDefeat = TestWorld->GetTimeSeconds();
        TestWorld->Tick(LEVELTICK_All, 0.5f);
        TestTrue(TEXT("The world continues after defeat"), TestWorld->GetTimeSeconds() > WorldTimeAtDefeat);
        TestEqual(TEXT("Defeat time remains fixed as the world advances"), Player->GetCombatElapsedSeconds(), DefeatTime);
        Player->Destroy();
    }
    TestWorld->EndPlay(EEndPlayReason::Quit);
    TestWorld->DestroyWorld(false);
    GEngine->DestroyWorldContext(TestWorld);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FParryActionDodgeReadinessTest,
    "ParryAction.Combat.DodgeReadiness",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FParryActionDodgeReadinessTest::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("Engine is available"), GEngine)) return false;
    UClass* PlayerClass = LoadClass<AParryActionCharacter>(nullptr,
        TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
    if (!TestNotNull(TEXT("Load player Blueprint"), PlayerClass)) return false;
    const FName WorldName = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("ParryActionDodgeTestWorld"));
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
    if (!TestNotNull(TEXT("Create isolated test world"), TestWorld)) return false;
    FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
    Context.SetCurrentWorld(TestWorld);
    UGameInstance* TestGameInstance = NewObject<UGameInstance>(GEngine);
    TestWorld->SetGameInstance(TestGameInstance);
    TestWorld->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
    TestTrue(TEXT("Create a game mode"), TestWorld->SetGameMode(FURL()));
    TestWorld->InitializeActorsForPlay(FURL());
    TestWorld->BeginPlay();
    TestWorld->GetWorldSettings()->NotifyBeginPlay();

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AParryActionCharacter* Player = TestWorld->SpawnActor<AParryActionCharacter>(PlayerClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);
    APlayerController* Controller = TestWorld->SpawnActor<APlayerController>(APlayerController::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (TestNotNull(TEXT("Spawn player"), Player) && TestNotNull(TEXT("Spawn test controller"), Controller))
    {
        TestFalse(TEXT("No controller means dodge is unavailable"), Player->IsDodgeReady());
        Controller->Possess(Player);
        Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        Player->DodgeCooldown = 0.7f;
        TestTrue(TEXT("Grounded player can dodge"), Player->IsDodgeReady());
        Player->Dodge();
        TestFalse(TEXT("Dodge enters cooldown"), Player->IsDodgeReady());
        TestEqual(TEXT("Cooldown bar starts empty"), Player->GetDodgeCooldownFraction(), 0.0f);
        const float NextDodge = Player->NextDodgeTime;
        Player->Dodge();
        TestEqual(TEXT("Repeated dodge cannot reset the cooldown"), Player->NextDodgeTime, NextDodge);
        Player->NextDodgeTime = TestWorld->GetTimeSeconds() + Player->DodgeCooldown * 0.5f;
        TestTrue(TEXT("Cooldown bar is half full"), FMath::IsNearlyEqual(Player->GetDodgeCooldownFraction(), 0.5f));
        Player->NextDodgeTime = TestWorld->GetTimeSeconds();
        TestEqual(TEXT("Cooldown bar is full when time elapsed"), Player->GetDodgeCooldownFraction(), 1.0f);
        TestTrue(TEXT("Grounded player can dodge after cooldown"), Player->IsDodgeReady());
        Player->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
        TestFalse(TEXT("Dodge is unavailable in the air even after cooldown"), Player->IsDodgeReady());
        Player->HandleVictory();
        TestFalse(TEXT("Dodge is unavailable after victory"), Player->IsDodgeReady());
        Controller->Destroy();
        Player->Destroy();
    }
    TestWorld->EndPlay(EEndPlayReason::Quit);
    TestWorld->DestroyWorld(false);
    GEngine->DestroyWorldContext(TestWorld);
    return !HasAnyErrors();
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FParryActionAttackDamageTest,
    "ParryAction.Combat.PlayerAttackDamage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FParryActionAttackDamageTest::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("Engine is available"), GEngine)) return false;
    UClass* PlayerClass = LoadClass<AParryActionCharacter>(nullptr,
        TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
    UClass* EnemyClass = LoadClass<ATrainingEnemy>(nullptr,
        TEXT("/Game/ThirdPerson/Blueprints/BP_TrainingEnemy.BP_TrainingEnemy_C"));
    if (!TestNotNull(TEXT("Load player Blueprint"), PlayerClass) || !TestNotNull(TEXT("Load enemy Blueprint"), EnemyClass)) return false;
    const FName WorldName = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("ParryActionPlayerDamageTestWorld"));
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
    if (!TestNotNull(TEXT("Create isolated test world"), TestWorld)) return false;
    FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
    Context.SetCurrentWorld(TestWorld);
    UGameInstance* TestGameInstance = NewObject<UGameInstance>(GEngine);
    TestWorld->SetGameInstance(TestGameInstance);
    TestWorld->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
    TestTrue(TEXT("Create a game mode"), TestWorld->SetGameMode(FURL()));
    TestWorld->InitializeActorsForPlay(FURL());
    TestWorld->BeginPlay();
    TestWorld->GetWorldSettings()->NotifyBeginPlay();

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AParryActionCharacter* Player = TestWorld->SpawnActor<AParryActionCharacter>(PlayerClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);
    ATrainingEnemy* Enemy = TestWorld->SpawnActor<ATrainingEnemy>(EnemyClass, FVector(150, 0, 0), FRotator::ZeroRotator, Params);
    if (TestNotNull(TEXT("Spawn player"), Player) && TestNotNull(TEXT("Spawn enemy"), Enemy))
    {
        TestNull(TEXT("No player controller or personal save"), TestWorld->GetFirstPlayerController());
        Player->AttackHitEffectAsset.Reset();
        Player->AttackHitSoundAsset.Reset();
        Player->AttackHitCameraShake = nullptr;
        TestEqual(TEXT("Enemy starts with 160 health"), Enemy->GetCurrentHealth(), 160.0f);
        Player->ResolveAttack();
        TestEqual(TEXT("Normal attack deals 10 damage"), Enemy->GetCurrentHealth(), 150.0f);
        TestFalse(TEXT("Normal hit is not a counter"), Player->DidLastAttackCounter());
        Enemy->bStunned = true;
        Player->ResolveAttack();
        TestEqual(TEXT("Counter attack deals 20 damage"), Enemy->GetCurrentHealth(), 130.0f);
        TestTrue(TEXT("Hit against stunned enemy is a counter"), Player->DidLastAttackCounter());
        Enemy->Destroy();
        Player->Destroy();
    }
    TestWorld->EndPlay(EEndPlayReason::Quit);
    TestWorld->DestroyWorld(false);
    GEngine->DestroyWorldContext(TestWorld);
    return !HasAnyErrors();
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FParryActionComboBufferTest,
    "ParryAction.Combat.ComboBuffer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Timers are processed once per engine frame, so advance the isolated world across frames.
class FComboBufferLatentCommand : public IAutomationLatentCommand
{
public:
    FComboBufferLatentCommand(FParryActionComboBufferTest* InTest, UWorld* InWorld,
        AParryActionCharacter* InPlayer, ATrainingEnemy* InEnemy)
        : Test(InTest), World(InWorld), Player(InPlayer), Enemy(InEnemy) {}

    virtual bool Update() override
    {
        World->Tick(LEVELTICK_All, 0.1f);
        ++Frames;
        const bool bSecondStarted = Player->ActiveAttackAnimation == Player->SecondAttackAnimation;
        if (!bSecondStarted && Frames < 30) return false;
        Test->TestTrue(TEXT("Buffered second attack starts after the first hit and cooldown"), bSecondStarted);
        if (bSecondStarted)
        {
            Test->TestFalse(TEXT("Buffered input is consumed"), Player->bSecondAttackQueued);
            Test->TestTrue(TEXT("Second attack is awaiting its hit notify"), Player->bAttackHitPending);
            Player->HandleAttackHitNotify(Player->SecondAttackAnimation);
            Test->TestEqual(TEXT("Second hit applies exactly one more attack"), Enemy->GetCurrentHealth(), 140.0f);
            Player->HandleAttackHitNotify(Player->SecondAttackAnimation);
            Test->TestEqual(TEXT("Duplicate notify does not apply another hit"), Enemy->GetCurrentHealth(), 140.0f);
        }
        Enemy->Destroy();
        Player->Destroy();
        World->EndPlay(EEndPlayReason::Quit);
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
        return true;
    }
private:
    FParryActionComboBufferTest* Test;
    UWorld* World;
    AParryActionCharacter* Player;
    ATrainingEnemy* Enemy;
    int32 Frames = 0;
};

bool FParryActionComboBufferTest::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("Engine is available"), GEngine)) return false;
    UClass* PlayerClass = LoadClass<AParryActionCharacter>(nullptr,
        TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
    UClass* EnemyClass = LoadClass<ATrainingEnemy>(nullptr,
        TEXT("/Game/ThirdPerson/Blueprints/BP_TrainingEnemy.BP_TrainingEnemy_C"));
    if (!TestNotNull(TEXT("Load player Blueprint"), PlayerClass) || !TestNotNull(TEXT("Load enemy Blueprint"), EnemyClass)) return false;
    const FName WorldName = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("ParryActionComboBufferTestWorld"));
    UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
    if (!TestNotNull(TEXT("Create isolated test world"), TestWorld)) return false;
    FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
    Context.SetCurrentWorld(TestWorld);
    UGameInstance* TestGameInstance = NewObject<UGameInstance>(GEngine);
    TestWorld->SetGameInstance(TestGameInstance);
    TestWorld->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
    TestTrue(TEXT("Create a game mode"), TestWorld->SetGameMode(FURL()));
    TestWorld->InitializeActorsForPlay(FURL());
    TestWorld->BeginPlay();
    TestWorld->GetWorldSettings()->NotifyBeginPlay();

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AParryActionCharacter* Player = TestWorld->SpawnActor<AParryActionCharacter>(PlayerClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);
    ATrainingEnemy* Enemy = TestWorld->SpawnActor<ATrainingEnemy>(EnemyClass, FVector(150, 0, 0), FRotator::ZeroRotator, Params);
    if (TestNotNull(TEXT("Spawn player"), Player) && TestNotNull(TEXT("Spawn enemy"), Enemy)
        && TestNotNull(TEXT("Player mesh exists"), Player->GetMesh())
        && TestNotNull(TEXT("Player animation instance exists"), Player->GetMesh()->GetAnimInstance())
        && TestNotNull(TEXT("First attack animation assigned"), Player->AttackAnimation.Get())
        && TestNotNull(TEXT("Second attack animation assigned"), Player->SecondAttackAnimation.Get()))
    {
        TestNull(TEXT("No player controller or personal save"), TestWorld->GetFirstPlayerController());
        Player->GetCharacterMovement()->DisableMovement();
        // Synthetic notifies isolate the queue logic from headless rendering and animation timing.
        Player->GetMesh()->SetComponentTickEnabled(false);
        Player->AttackHitEffectAsset.Reset();
        Player->AttackHitSoundAsset.Reset();
        Player->AttackHitCameraShake = nullptr;
        Player->Attack();
        TestTrue(TEXT("First attack is awaiting its notify"), Player->bAttackHitPending);
        TestTrue(TEXT("First animation is active"), Player->ActiveAttackAnimation == Player->AttackAnimation);
        Player->Attack();
        Player->Attack();
        TestTrue(TEXT("Early input queues only one second attack"), Player->bSecondAttackQueued);
        Player->HandleAttackHitNotify(Player->SecondAttackAnimation);
        TestTrue(TEXT("Wrong animation notify cannot resolve the first hit"), Player->bAttackHitPending);
        TestEqual(TEXT("Wrong notify does not cause damage"), Enemy->GetCurrentHealth(), 160.0f);
        Player->HandleAttackHitNotify(Player->AttackAnimation);
        TestEqual(TEXT("First notify applies exactly one hit"), Enemy->GetCurrentHealth(), 150.0f);
        TestTrue(TEXT("Second input waits for the attack cooldown"), Player->bSecondAttackQueued);
        ADD_LATENT_AUTOMATION_COMMAND(FComboBufferLatentCommand(this, TestWorld, Player, Enemy));
        return true;
    }
    TestWorld->EndPlay(EEndPlayReason::Quit);
    TestWorld->DestroyWorld(false);
    GEngine->DestroyWorldContext(TestWorld);
    return !HasAnyErrors();
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FParryActionAnimationHitTest,
    "ParryAction.Combat.AnimationHitIntegration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Tick the real mesh and its animation in an isolated game world. Unlike the
// combo-buffer unit test, this test never calls HandleAttackHitNotify itself.
class FAnimationHitLatentCommand : public IAutomationLatentCommand
{
public:
    FAnimationHitLatentCommand(FParryActionAnimationHitTest* InTest, UWorld* InWorld,
        AParryActionCharacter* InPlayer, ATrainingEnemy* InEnemy)
        : Test(InTest), World(InWorld), Player(InPlayer), Enemy(InEnemy) {}

    virtual bool Update() override
    {
        World->Tick(LEVELTICK_All, 0.05f);
        ++Frames;
        const float Health = Enemy->GetCurrentHealth();
        if (!bSawFirstHit && Health < 160.0f)
        {
            bSawFirstHit = true;
            Test->TestEqual(TEXT("First animation notify causes exactly 10 damage"), Health, 150.0f);
        }
        if (Health <= 140.0f || Frames >= 80)
        {
            Test->TestTrue(TEXT("First attack animation delivered a hit"), bSawFirstHit);
            Test->TestEqual(TEXT("Both animation notifies cause exactly two hits"), Health, 140.0f);
            Enemy->Destroy();
            Player->Destroy();
            World->EndPlay(EEndPlayReason::Quit);
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
            return true;
        }
        return false;
    }
private:
    FParryActionAnimationHitTest* Test;
    UWorld* World;
    AParryActionCharacter* Player;
    ATrainingEnemy* Enemy;
    int32 Frames = 0;
    bool bSawFirstHit = false;
};

bool FParryActionAnimationHitTest::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("Engine is available"), GEngine)) return false;
    UClass* PlayerClass = LoadClass<AParryActionCharacter>(nullptr,
        TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
    UClass* EnemyClass = LoadClass<ATrainingEnemy>(nullptr,
        TEXT("/Game/ThirdPerson/Blueprints/BP_TrainingEnemy.BP_TrainingEnemy_C"));
    if (!TestNotNull(TEXT("Load player Blueprint"), PlayerClass) || !TestNotNull(TEXT("Load enemy Blueprint"), EnemyClass)) return false;
    const FName Name = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), TEXT("AnimationHitTestWorld"));
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, Name, GetTransientPackage());
    if (!TestNotNull(TEXT("Create isolated test world"), World)) return false;
    FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
    Context.SetCurrentWorld(World);
    World->SetGameInstance(NewObject<UGameInstance>(GEngine));
    World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
    TestTrue(TEXT("Create game mode"), World->SetGameMode(FURL()));
    World->InitializeActorsForPlay(FURL());
    World->BeginPlay();
    World->GetWorldSettings()->NotifyBeginPlay();

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AParryActionCharacter* Player = World->SpawnActor<AParryActionCharacter>(PlayerClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);
    ATrainingEnemy* Enemy = World->SpawnActor<ATrainingEnemy>(EnemyClass, FVector(150, 0, 0), FRotator::ZeroRotator, Params);
    if (TestNotNull(TEXT("Spawn player"), Player) && TestNotNull(TEXT("Spawn enemy"), Enemy)
        && TestNotNull(TEXT("Player mesh"), Player->GetMesh())
        && TestNotNull(TEXT("Player animation instance"), Player->GetMesh()->GetAnimInstance())
        && TestNotNull(TEXT("First attack animation"), Player->AttackAnimation.Get())
        && TestNotNull(TEXT("Second attack animation"), Player->SecondAttackAnimation.Get()))
    {
        TestNull(TEXT("No player controller or personal save"), World->GetFirstPlayerController());
        Player->GetCharacterMovement()->DisableMovement();
        Player->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        Player->GetMesh()->SetComponentTickEnabled(true);
        Player->AttackHitEffectAsset.Reset();
        Player->AttackHitSoundAsset.Reset();
        Player->AttackHitCameraShake = nullptr;
        TestEqual(TEXT("Enemy starts at 160 health"), Enemy->GetCurrentHealth(), 160.0f);
        Player->Attack();
        Player->Attack();
        TestTrue(TEXT("Second attack is buffered"), Player->bSecondAttackQueued);
        ADD_LATENT_AUTOMATION_COMMAND(FAnimationHitLatentCommand(this, World, Player, Enemy));
        return true;
    }
    World->EndPlay(EEndPlayReason::Quit);
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return !HasAnyErrors();
}

#endif
