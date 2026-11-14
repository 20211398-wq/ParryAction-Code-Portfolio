#include "TrainingEnemy.h"
#include "ParryActionCharacter.h"
#include "ParryActionPlayerController.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequenceBase.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ATrainingEnemy::ATrainingEnemy()
{
    PrimaryActorTick.bCanEverTick = false;
    EnemyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EnemyMesh"));
    RootComponent = EnemyMesh;
    EnemyMesh->SetMobility(EComponentMobility::Movable);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (Cylinder.Succeeded())
    {
        EnemyMesh->SetStaticMesh(Cylinder.Object);
    }
    EnemyMesh->SetRelativeScale3D(FVector(0.7f, 0.7f, 1.6f));
    EnemyMesh->SetCollisionProfileName(TEXT("BlockAll"));

    // Keep the existing cylinder as the collision root; the mannequin is visual only.
    EnemyVisualMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("EnemyVisualMesh"));
    EnemyVisualMesh->SetupAttachment(EnemyMesh);
    EnemyVisualMesh->SetAbsolute(false, false, true);
    EnemyVisualMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -50.0f));
    EnemyVisualMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    EnemyVisualMesh->SetRelativeScale3D(FVector::OneVector);
    EnemyVisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    EnemyVisualMesh->SetGenerateOverlapEvents(false);
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Mannequin(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
    if (Mannequin.Succeeded())
    {
        EnemyVisualMesh->SetSkeletalMeshAsset(Mannequin.Object);
    }
    static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Idle(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
    static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Walk(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd.MF_Unarmed_Walk_Fwd"));
    if (Idle.Succeeded()) { IdleAnimation = Idle.Object; }
    if (Walk.Succeeded()) { WalkAnimation = Walk.Object; }
    static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> AttackVisual(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01"));
    if (AttackVisual.Succeeded()) { AttackVisualAnimation = AttackVisual.Object; }
    static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> UnblockableVisual(TEXT("/Game/ParryAction/Animations/AS_EnemyUnblockable.AS_EnemyUnblockable"));
    if (UnblockableVisual.Succeeded()) { UnblockableAttackVisualAnimation = UnblockableVisual.Object; }
    static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> SecondPunch(TEXT("/Game/ParryAction/Animations/AS_SecondAttack.AS_SecondAttack"));
    if (SecondPunch.Succeeded()) { UnblockableSecondVisualAnimation = SecondPunch.Object; }
    static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> StunVisual(TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/MM_HitReact_Front_Med_01.MM_HitReact_Front_Med_01"));
    if (StunVisual.Succeeded()) { ParryStunAnimation = StunVisual.Object; }
    static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> DefeatVisual(TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Back_01.MM_Death_Back_01"));
    if (DefeatVisual.Succeeded()) { DefeatAnimation = DefeatVisual.Object; }
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Game/ParryAction/Materials/M_TrainingEnemyState.M_TrainingEnemyState"));
    if (Material.Succeeded())
    {
        StateMaterial = Material.Object;
    }
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Overlay(TEXT("/Game/ParryAction/Materials/M_EnemyWarningOverlay.M_EnemyWarningOverlay"));
    if (Overlay.Succeeded())
    {
        WarningOverlayMaterial = Overlay.Object;
    }
    // Keep a soft reference so Niagara is not loaded while native class defaults are being created.
    ParrySuccessEffectAsset = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Variant_Combat/VFX/NS_Damage.NS_Damage")));
    ParrySuccessSoundAsset = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/ParryAction/Audio/SFX_ParrySuccess.SFX_ParrySuccess")));
}

void ATrainingEnemy::BeginPlay()
{
    Super::BeginPlay();
    MaxHealth = FMath::Max(1.0f, MaxHealth);
    CurrentHealth = MaxHealth;
    bDefeated = false;
    bStunned = false;
    bWindingUp = false;
    CurrentAttackType = ETrainingAttackType::Parryable;
    NextAttackType = ETrainingAttackType::Parryable;
    ConsecutiveAttackCount = 0;
    UnblockableStrikeIndex = 0;
    bLungeDirectionLocked = false;
    if (StateMaterial && EnemyMesh)
    {
        StateMaterialInstance = EnemyMesh->CreateDynamicMaterialInstance(0, StateMaterial);
        UpdateStateAppearance();
    }
    if (EnemyVisualMesh && EnemyVisualMesh->GetSkeletalMeshAsset())
    {
        // Align only the visual feet to the ground; leave the collision root untouched.
        FHitResult FloorHit;
        FCollisionQueryParams FloorParams(SCENE_QUERY_STAT(TrainingEnemyVisualFloor), false, this);
        const FVector Origin = GetActorLocation();
        const FVector RootScale = EnemyMesh->GetComponentScale();
        if (GetWorld() && !FMath::IsNearlyZero(RootScale.Z)
            && GetWorld()->LineTraceSingleByChannel(FloorHit, Origin, Origin - FVector(0.0f, 0.0f, 250.0f), ECC_Visibility, FloorParams)
            && FloorHit.ImpactNormal.Z > 0.9f)
        {
            FVector VisualOffset = EnemyVisualMesh->GetRelativeLocation();
            VisualOffset.Z = (FloorHit.ImpactPoint.Z - Origin.Z + 1.0f) / RootScale.Z;
            EnemyVisualMesh->SetRelativeLocation(VisualOffset);
        }
        // Hiding the root mesh must not hide the attached visual component.
        EnemyMesh->SetHiddenInGame(true, false);
        if (WarningOverlayMaterial)
        {
            WarningOverlayInstance = UMaterialInstanceDynamic::Create(WarningOverlayMaterial, this);
            EnemyVisualMesh->SetOverlayMaterial(WarningOverlayInstance);
            UpdateStateAppearance();
        }
        else if (StateMaterialInstance)
        {
            // Preserve the previous readable fallback if the overlay is missing.
            for (int32 Slot = 0; Slot < EnemyVisualMesh->GetNumMaterials(); ++Slot)
            {
                EnemyVisualMesh->SetMaterial(Slot, StateMaterialInstance);
            }
        }
        LastVisualLocation = GetActorLocation();
        bVisualWalking = false;
        bAttackVisualPlaying = false;
        if (IdleAnimation)
        {
            EnemyVisualMesh->PlayAnimation(IdleAnimation, true);
        }
        UpdateVisualMovement();
        GetWorldTimerManager().SetTimer(VisualTimer, this, &ATrainingEnemy::UpdateVisualMovement, 0.1f, true);
    }
    GetWorldTimerManager().SetTimer(DetectionTimer, this, &ATrainingEnemy::CheckForPlayer, 0.2f, true);
    // The existing Blueprint was created before pursuit existed. A timer does
    // not depend on whether that Blueprint inherited the native tick flag.
    GetWorldTimerManager().SetTimer(PursuitTimer, this, &ATrainingEnemy::UpdatePursuit, 1.0f / 30.0f, true);
}

void ATrainingEnemy::UpdateStateAppearance()
{
    if (!StateMaterialInstance && !WarningOverlayInstance)
    {
        return;
    }
    const FLinearColor Color = bStunned ? StunnedColor
        : (bWindingUp ? (CurrentAttackType == ETrainingAttackType::Unblockable ? UnblockableColor : ParryableColor) : IdleColor);
    // Lighten the existing state color briefly so the warning colors remain visible.
    const FLinearColor DisplayColor = bHitFlashActive ? FMath::Lerp(Color, FLinearColor::White, 0.45f) : Color;
    if (StateMaterialInstance)
    {
        StateMaterialInstance->SetVectorParameterValue(TEXT("StateColor"), DisplayColor);
    }
    if (WarningOverlayInstance)
    {
        // No tint at rest; retain attack, stun, and hit-flash readability.
        const FLinearColor OverlayColor = (bWindingUp || bStunned || bHitFlashActive) ? DisplayColor : FLinearColor::Black;
        WarningOverlayInstance->SetVectorParameterValue(TEXT("StateColor"), OverlayColor);
    }
}

void ATrainingEnemy::EndHitFlash()
{
    bHitFlashActive = false;
    UpdateStateAppearance();
}

void ATrainingEnemy::UpdateVisualMovement()
{
    if (!EnemyVisualMesh || bDefeated)
    {
        return;
    }
    const FVector CurrentLocation = GetActorLocation();
    const bool bMoved = FVector::DistSquared2D(CurrentLocation, LastVisualLocation) > 1.0f;
    LastVisualLocation = CurrentLocation;
    const bool bShouldWalk = bMoved && !bStunned && !bWindingUp;
    if (!bAttackVisualPlaying && !bStunVisualPlaying && bShouldWalk != bVisualWalking)
    {
        bVisualWalking = bShouldWalk;
        UAnimSequenceBase* Animation = bVisualWalking ? WalkAnimation.Get() : IdleAnimation.Get();
        if (Animation)
        {
            EnemyVisualMesh->PlayAnimation(Animation, true);
        }
    }
    const UWorld* World = GetWorld();
    const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
    const AParryActionCharacter* Player = Controller ? Cast<AParryActionCharacter>(Controller->GetPawn()) : nullptr;
    if (IsValid(Player) && !Player->IsDefeated())
    {
        FVector Direction = bWindingUp && bLungeDirectionLocked
            ? LockedLungeDirection : Player->GetActorLocation() - CurrentLocation;
        Direction.Z = 0.0f;
        if (!Direction.IsNearlyZero())
        {
            // The template mannequin faces -Y in its mesh coordinate system.
            EnemyVisualMesh->SetWorldRotation(FRotator(0.0f, Direction.Rotation().Yaw - 90.0f, 0.0f));
        }
    }
}

void ATrainingEnemy::StartAttackVisual()
{
    if (bDefeated || bStunned || !bWindingUp || !EnemyVisualMesh)
    {
        return;
    }
    UAnimSequenceBase* Visual = AttackVisualAnimation.Get();
    if (CurrentAttackType == ETrainingAttackType::Unblockable)
    {
        Visual = UnblockableStrikeIndex == 1 && UnblockableSecondVisualAnimation
            ? UnblockableSecondVisualAnimation.Get()
            : (UnblockableAttackVisualAnimation ? UnblockableAttackVisualAnimation.Get() : AttackVisualAnimation.Get());
    }
    if (!Visual)
    {
        return;
    }
    bAttackVisualPlaying = true;
    EnemyVisualMesh->PlayAnimation(Visual, false);
    GetWorldTimerManager().SetTimer(AttackVisualEndTimer, this, &ATrainingEnemy::EndAttackVisual,
        FMath::Max(0.05f, Visual->GetPlayLength()), false);
}

void ATrainingEnemy::EndAttackVisual()
{
    GetWorldTimerManager().ClearTimer(AttackVisualEndTimer);
    bAttackVisualPlaying = false;
    bVisualWalking = false;
    if (!bDefeated && EnemyVisualMesh && IdleAnimation)
    {
        EnemyVisualMesh->PlayAnimation(IdleAnimation, true);
    }
}

void ATrainingEnemy::StartParryStunVisual()
{
    if (bDefeated || !bStunned || !EnemyVisualMesh || !ParryStunAnimation)
    {
        return;
    }
    bStunVisualPlaying = true;
    EnemyVisualMesh->PlayAnimation(ParryStunAnimation, false);
    GetWorldTimerManager().SetTimer(StunVisualEndTimer, this, &ATrainingEnemy::EndParryStunVisual,
        FMath::Max(0.05f, ParryStunAnimation->GetPlayLength()), false);
}

void ATrainingEnemy::EndParryStunVisual()
{
    GetWorldTimerManager().ClearTimer(StunVisualEndTimer);
    bStunVisualPlaying = false;
    bVisualWalking = false;
    if (!bDefeated && EnemyVisualMesh && IdleAnimation)
    {
        EnemyVisualMesh->PlayAnimation(IdleAnimation, true);
    }
}

void ATrainingEnemy::HideDefeatedEnemy()
{
    if (bDefeated)
    {
        SetActorHiddenInGame(true);
    }
}

void ATrainingEnemy::UpdatePursuit()
{
    UWorld* World = GetWorld();
    if (!World || bDefeated || bStunned || bWindingUp || bDangerChallengeActive)
    {
        return;
    }
    const APlayerController* PlayerController = World->GetFirstPlayerController();
    const AParryActionCharacter* Player = PlayerController ? Cast<AParryActionCharacter>(PlayerController->GetPawn()) : nullptr;
    if (!IsValid(Player) || Player->IsDefeated())
    {
        return;
    }
    const FVector Position = GetActorLocation();
    FVector ToPlayer = Player->GetActorLocation() - Position;
    if (FMath::Abs(ToPlayer.Z) > 120.0f)
    {
        return;
    }
    ToPlayer.Z = 0.0f;
    const float Distance = ToPlayer.Size();
    const float StopDistance = FMath::Max(1.0f, ChaseStopDistance);
    if (Distance <= StopDistance || Distance > FMath::Max(StopDistance, DetectionRange))
    {
        return;
    }
    const float PursuitInterval = GetWorldTimerManager().GetTimerRate(PursuitTimer);
    const float Step = FMath::Min(FMath::Max(0.0f, ChaseSpeed) * FMath::Max(0.0f, PursuitInterval), Distance - StopDistance);
    if (Step <= 0.0f)
    {
        return;
    }
    MoveOnSafeGround(ToPlayer.GetSafeNormal(), Step, Player);
}

bool ATrainingEnemy::MoveOnSafeGround(const FVector& Direction, float Distance, const AParryActionCharacter* Player)
{
    UWorld* World = GetWorld();
    if (!World || Direction.IsNearlyZero() || Distance <= 0.0f)
    {
        return false;
    }
    const FVector Position = GetActorLocation();
    const FVector Destination = Position + Direction * Distance;

    // The prototype does not have navigation or gravity. Only move when both
    // points have nearby, nearly level walkable ground; sweep blocks walls.
    FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(TrainingEnemyGround), false, this);
    GroundParams.AddIgnoredActor(Player);
    FHitResult CurrentGround;
    FHitResult DestinationGround;
    FHitResult FrontGround;
    const FVector GroundTop(0.0f, 0.0f, 20.0f);
    const FVector GroundBottom(0.0f, 0.0f, 250.0f);
    const FVector Front = Destination + Direction * 35.0f;
    const bool bHasGround = World->LineTraceSingleByChannel(CurrentGround, Position + GroundTop, Position - GroundBottom, ECC_Visibility, GroundParams)
        && World->LineTraceSingleByChannel(DestinationGround, Destination + GroundTop, Destination - GroundBottom, ECC_Visibility, GroundParams)
        && World->LineTraceSingleByChannel(FrontGround, Front + GroundTop, Front - GroundBottom, ECC_Visibility, GroundParams);
    if (!bHasGround || CurrentGround.ImpactNormal.Z < 0.99f || DestinationGround.ImpactNormal.Z < 0.99f || FrontGround.ImpactNormal.Z < 0.99f
        || FMath::Abs(CurrentGround.ImpactPoint.Z - DestinationGround.ImpactPoint.Z) > 5.0f
        || FMath::Abs(CurrentGround.ImpactPoint.Z - FrontGround.ImpactPoint.Z) > 5.0f)
    {
        if (World->GetTimeSeconds() >= NextPursuitDiagnosticTime)
        {
            NextPursuitDiagnosticTime = World->GetTimeSeconds() + 5.0f;
            UE_LOG(LogTemp, Log, TEXT("Training enemy pursuit blocked by ground check (ground found: %d)."), bHasGround ? 1 : 0);
        }
        return false;
    }
    FHitResult MovementHit;
    AddActorWorldOffset(Destination - Position, true, &MovementHit);
    if (MovementHit.bBlockingHit && World->GetTimeSeconds() >= NextPursuitDiagnosticTime)
    {
        NextPursuitDiagnosticTime = World->GetTimeSeconds() + 5.0f;
        UE_LOG(LogTemp, Log, TEXT("Training enemy movement blocked by %s (initial overlap: %d)."), *GetNameSafe(MovementHit.GetActor()), MovementHit.bStartPenetrating ? 1 : 0);
    }
    return !MovementHit.bBlockingHit;
}

void ATrainingEnemy::ScheduleUnblockableLunge(float TimeUntilHit)
{
    const float Duration = FMath::Min(FMath::Max(0.05f, UnblockableLungeDuration), TimeUntilHit);
    const float Delay = TimeUntilHit - Duration;
    if (Delay > KINDA_SMALL_NUMBER)
    {
        GetWorldTimerManager().SetTimer(LungeStartTimer, this, &ATrainingEnemy::BeginUnblockableLunge, Delay, false);
    }
    else
    {
        BeginUnblockableLunge();
    }
}

void ATrainingEnemy::BeginUnblockableLunge()
{
    AParryActionCharacter* Character = AttackTarget.Get();
    if (bDefeated || !bWindingUp || !IsValid(Character) || Character->IsDefeated())
    {
        StopUnblockableLunge();
        return;
    }
    if (!bLungeDirectionLocked)
    {
        LockedLungeDirection = Character->GetActorLocation() - GetActorLocation();
        LockedLungeDirection.Z = 0.0f;
        LockedLungeDirection = LockedLungeDirection.GetSafeNormal();
        bLungeDirectionLocked = !LockedLungeDirection.IsNearlyZero();
    }
    LungeDistanceRemaining = FMath::Max(0.0f, UnblockableLungeDistance);
    if (bLungeDirectionLocked && LungeDistanceRemaining > 0.0f)
    {
        GetWorldTimerManager().SetTimer(LungeMovementTimer, this, &ATrainingEnemy::UpdateUnblockableLunge, 1.0f / 30.0f, true);
    }
}

void ATrainingEnemy::UpdateUnblockableLunge()
{
    AParryActionCharacter* Character = AttackTarget.Get();
    if (bDefeated || !bWindingUp || !IsValid(Character) || Character->IsDefeated())
    {
        StopUnblockableLunge();
        return;
    }
    const float Step = FMath::Min(LungeDistanceRemaining,
        FMath::Max(0.0f, UnblockableLungeDistance) / FMath::Max(0.05f, UnblockableLungeDuration) / 30.0f);
    if (Step <= KINDA_SMALL_NUMBER || !MoveOnSafeGround(LockedLungeDirection, Step, Character))
    {
        StopUnblockableLunge();
        return;
    }
    LungeDistanceRemaining -= Step;
    if (LungeDistanceRemaining <= KINDA_SMALL_NUMBER)
    {
        StopUnblockableLunge();
    }
}

void ATrainingEnemy::StopUnblockableLunge()
{
    GetWorldTimerManager().ClearTimer(LungeStartTimer);
    GetWorldTimerManager().ClearTimer(LungeMovementTimer);
    LungeDistanceRemaining = 0.0f;
}

void ATrainingEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CancelDangerChallenge();
    GetWorldTimerManager().ClearTimer(HitFlashTimer);
    GetWorldTimerManager().ClearTimer(DefeatVisualEndTimer);
    GetWorldTimerManager().ClearTimer(StunVisualEndTimer);
    GetWorldTimerManager().ClearTimer(DetectionTimer);
    GetWorldTimerManager().ClearTimer(PursuitTimer);
    GetWorldTimerManager().ClearTimer(VisualTimer);
    GetWorldTimerManager().ClearTimer(AttackVisualStartTimer);
    GetWorldTimerManager().ClearTimer(AttackVisualEndTimer);
    GetWorldTimerManager().ClearTimer(WindupTimer);
    StopUnblockableLunge();
    GetWorldTimerManager().ClearTimer(StunTimer);
    Super::EndPlay(EndPlayReason);
}



float ATrainingEnemy::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    if (bDefeated || DamageAmount <= 0.0f)
    {
        return 0.0f;
    }
    const float AppliedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    if (AppliedDamage <= 0.0f)
    {
        return 0.0f;
    }
    const float PreviousHealth = CurrentHealth;
    CurrentHealth = FMath::Clamp(CurrentHealth - AppliedDamage, 0.0f, MaxHealth);
    if (CurrentHealth <= 0.0f)
    {
        bDefeated = true;
        CancelDangerChallenge();
        bHitFlashActive = false;
        GetWorldTimerManager().ClearTimer(HitFlashTimer);
        if (const UWorld* World = GetWorld())
        {
            if (AParryActionPlayerController* PlayerController = Cast<AParryActionPlayerController>(World->GetFirstPlayerController()))
            {
                PlayerController->StopCombatMusic();
                PlayerController->RecordVictory();
                if (AParryActionCharacter* PlayerCharacter = Cast<AParryActionCharacter>(PlayerController->GetPawn()))
                {
                    PlayerCharacter->HandleVictory();
                }
            }
        }
        GetWorldTimerManager().ClearTimer(StunVisualEndTimer);
        bStunVisualPlaying = false;
        bWindingUp = false;
        bStunned = false;
        AttackTarget.Reset();
        GetWorldTimerManager().ClearTimer(DetectionTimer);
        GetWorldTimerManager().ClearTimer(PursuitTimer);
        GetWorldTimerManager().ClearTimer(VisualTimer);
        GetWorldTimerManager().ClearTimer(AttackVisualStartTimer);
        GetWorldTimerManager().ClearTimer(AttackVisualEndTimer);
        GetWorldTimerManager().ClearTimer(WindupTimer);
        StopUnblockableLunge();
        bLungeDirectionLocked = false;
        GetWorldTimerManager().ClearTimer(StunTimer);
        SetActorEnableCollision(false);
        UpdateStateAppearance();
        if (EnemyVisualMesh && EnemyVisualMesh->GetSkeletalMeshAsset() && DefeatAnimation)
        {
            EnemyVisualMesh->PlayAnimation(DefeatAnimation, false);
            GetWorldTimerManager().SetTimer(DefeatVisualEndTimer, this, &ATrainingEnemy::HideDefeatedEnemy,
                FMath::Max(0.05f, DefeatAnimation->GetPlayLength()), false);
        }
        else
        {
            HideDefeatedEnemy();
        }
    }
    else if (CurrentHealth < PreviousHealth && HitFlashSeconds > 0.0f)
    {
        bHitFlashActive = true;
        UpdateStateAppearance();
        GetWorldTimerManager().SetTimer(HitFlashTimer, this, &ATrainingEnemy::EndHitFlash, HitFlashSeconds, false);
    }
    return PreviousHealth - CurrentHealth;
}

float ATrainingEnemy::GetParryStunFraction() const
{
    const UWorld* World = GetWorld();
    if (!World || !bStunned || bDefeated)
    {
        return 0.0f;
    }
    const FTimerManager& Timers = World->GetTimerManager();
    const float Duration = Timers.GetTimerRate(StunTimer);
    return Duration > 0.0f ? FMath::Clamp(Timers.GetTimerRemaining(StunTimer) / Duration, 0.0f, 1.0f) : 0.0f;
}

float ATrainingEnemy::GetAttackWindupFraction() const
{
    const UWorld* World = GetWorld();
    if (!World || !bWindingUp || bDefeated)
    {
        return 0.0f;
    }
    const FTimerManager& Timers = World->GetTimerManager();
    const float Duration = Timers.GetTimerRate(WindupTimer);
    return Duration > 0.0f ? FMath::Clamp(Timers.GetTimerRemaining(WindupTimer) / Duration, 0.0f, 1.0f) : 0.0f;
}

int32 ATrainingEnemy::GetRecentAttackOutcome() const
{
    const UWorld* World = GetWorld();
    return World && World->GetTimeSeconds() < AttackOutcomeEndTime ? LastAttackOutcome : 0;
}

bool ATrainingEnemy::CanReach(const AParryActionCharacter* Character) const
{
    const UWorld* World = GetWorld();
    if (bDefeated || !IsValid(Character) || Character->IsDefeated() || !World)
    {
        return false;
    }
    const FVector Offset = Character->GetActorLocation() - GetActorLocation();
    if (Offset.SizeSquared2D() > FMath::Square(FMath::Max(1.0f, AttackRange)) || FMath::Abs(Offset.Z) > 120.0f)
    {
        return false;
    }
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(TrainingEnemyLineOfSight), false, this);
    const bool bBlocked = World->LineTraceSingleByChannel(Hit, GetActorLocation(), Character->GetActorLocation(), ECC_Visibility, Params);
    return !bBlocked || Hit.GetActor() == Character;
}

ETrainingAttackType ATrainingEnemy::ChooseNextAttack(ETrainingAttackType Previous, int32 ConsecutiveCount, bool bPickUnblockable)
{
    if (ConsecutiveCount >= 2)
    {
        return Previous == ETrainingAttackType::Parryable ? ETrainingAttackType::Unblockable : ETrainingAttackType::Parryable;
    }
    return bPickUnblockable ? ETrainingAttackType::Unblockable : ETrainingAttackType::Parryable;
}

void ATrainingEnemy::CheckForPlayer()
{
    UWorld* World = GetWorld();
    if (!World || bDefeated || bStunned || bWindingUp || bDangerChallengeActive || World->GetTimeSeconds() < NextAttackTime)
    {
        return;
    }
    const APlayerController* PlayerController = World->GetFirstPlayerController();
    AParryActionCharacter* Character = PlayerController ? Cast<AParryActionCharacter>(PlayerController->GetPawn()) : nullptr;
    if (!CanReach(Character))
    {
        return;
    }
    AttackTarget = Character;
    UnblockableStrikeIndex = 0;
    bLungeDirectionLocked = false;
    LockedLungeDirection = FVector::ZeroVector;
    const ETrainingAttackType ScheduledAttack = NextAttackType;
    ConsecutiveAttackCount = ConsecutiveAttackCount > 0 && ScheduledAttack == CurrentAttackType
        ? ConsecutiveAttackCount + 1 : 1;
    CurrentAttackType = ScheduledAttack;
    NextAttackType = ChooseNextAttack(CurrentAttackType, ConsecutiveAttackCount, FMath::RandBool());
    bWindingUp = true;
    UpdateStateAppearance();
    const float Windup = FMath::Max(0.1f, CurrentAttackType == ETrainingAttackType::Unblockable ? UnblockableWindupSeconds : WindupSeconds * 0.75f);
    // Pick a visual strike point without changing either gameplay hit timer.
    const bool bHasUnblockableVisual = CurrentAttackType == ETrainingAttackType::Unblockable && UnblockableAttackVisualAnimation;
    const float VisualHitTime = bHasUnblockableVisual ? UnblockableVisualHitTime : AttackVisualHitTime;
    const float VisualDelay = Windup - FMath::Max(0.0f, VisualHitTime);
    if (VisualDelay > 0.0f)
    {
        GetWorldTimerManager().SetTimer(AttackVisualStartTimer, this, &ATrainingEnemy::StartAttackVisual, VisualDelay, false);
    }
    else
    {
        StartAttackVisual();
    }
    if (CurrentAttackType == ETrainingAttackType::Unblockable)
    {
        ScheduleUnblockableLunge(Windup);
    }
    GetWorldTimerManager().SetTimer(WindupTimer, this, &ATrainingEnemy::ResolveAttack, Windup, false);
}

ETrainingAttackOutcome ATrainingEnemy::ClassifyAttack(bool bInRange, ETrainingAttackType AttackType, bool bParryActive, bool bDodgeInvulnerable)
{
    if (!bInRange)
    {
        return ETrainingAttackOutcome::OutOfRange;
    }
    if (AttackType == ETrainingAttackType::Parryable && bParryActive)
    {
        return ETrainingAttackOutcome::Parried;
    }
    return bDodgeInvulnerable ? ETrainingAttackOutcome::Dodged : ETrainingAttackOutcome::PlayerHit;
}

void ATrainingEnemy::ResolveAttack()
{
    if (bDefeated)
    {
        return;
    }
    StopUnblockableLunge();
    AParryActionCharacter* Character = AttackTarget.Get();
    bool bInRange = CanReach(Character);
    if (bInRange && CurrentAttackType == ETrainingAttackType::Unblockable && bLungeDirectionLocked)
    {
        FVector ToCharacter = Character->GetActorLocation() - GetActorLocation();
        ToCharacter.Z = 0.0f;
        // A fixed forward arc lets a player evade by moving to the side.
        if (!ToCharacter.IsNearlyZero())
        {
            bInRange = FVector::DotProduct(LockedLungeDirection, ToCharacter.GetSafeNormal()) >= 0.5f;
        }
    }
    // A timed parry of the first danger strike starts the challenge. A dodge
    // remains the safe option and never starts it while invulnerability is active.
    if (bInRange && CurrentAttackType == ETrainingAttackType::Unblockable && UnblockableStrikeIndex == 0
        && Character->IsParryActive() && !Character->IsDodgeInvulnerable())
    {
        BeginDangerChallenge(Character);
        return;
    }
    const ETrainingAttackOutcome Outcome = ClassifyAttack(bInRange, CurrentAttackType,
        bInRange && Character->IsParryActive(), bInRange && Character->IsDodgeInvulnerable());
    const bool bParried = Outcome == ETrainingAttackOutcome::Parried;
    if (const UWorld* World = GetWorld())
    {
        LastAttackOutcome = static_cast<int32>(Outcome);
        AttackOutcomeEndTime = World->GetTimeSeconds() + 1.5f;
    }
    if (bParried)
    {
        // This branch only runs for an in-range, parryable hit during the parry window.
        if (IsValid(Character))
        {
            Character->RecordSuccessfulParry();
            if (USoundBase* ParrySound = ParrySuccessSoundAsset.LoadSynchronous())
            {
                UGameplayStatics::PlaySound2D(this, ParrySound, 0.5f);
            }
            if (UNiagaraSystem* ParryEffect = ParrySuccessEffectAsset.LoadSynchronous())
            {
                const FVector EffectLocation = (GetActorLocation() + Character->GetActorLocation()) * 0.5f + FVector(0.0f, 0.0f, 30.0f);
                UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ParryEffect, EffectLocation);
            }
        }
        bStunned = true;
        GetWorldTimerManager().ClearTimer(AttackVisualStartTimer);
        EndAttackVisual();
        StartParryStunVisual();
        const float StunDuration = FMath::Max(0.1f, ParryStunSeconds);
        GetWorldTimerManager().SetTimer(StunTimer, this, &ATrainingEnemy::EndParryStun, StunDuration, false);
    }
    UpdateStateAppearance();
    if (bInRange && !bParried)
    {
        const float Damage = CurrentAttackType == ETrainingAttackType::Unblockable ? UnblockableDamage * 0.5f : AttackDamage;
        UGameplayStatics::ApplyDamage(Character, FMath::Max(0.0f, Damage), nullptr, this, UDamageType::StaticClass());
    }
    if (CurrentAttackType == ETrainingAttackType::Unblockable && UnblockableStrikeIndex == 0
        && IsValid(Character) && !Character->IsDefeated())
    {
        UnblockableStrikeIndex = 1;
        const float Delay = FMath::Max(0.1f, UnblockableSecondHitDelay);
        const float VisualDelay = Delay - FMath::Max(0.0f, UnblockableSecondVisualHitTime);
        if (VisualDelay > 0.0f)
        {
            GetWorldTimerManager().SetTimer(AttackVisualStartTimer, this, &ATrainingEnemy::StartAttackVisual, VisualDelay, false);
        }
        else
        {
            StartAttackVisual();
        }
        ScheduleUnblockableLunge(Delay);
        GetWorldTimerManager().SetTimer(WindupTimer, this, &ATrainingEnemy::ResolveAttack, Delay, false);
        UpdateStateAppearance();
        return;
    }
    bWindingUp = false;
    bLungeDirectionLocked = false;
    if (UWorld* World = GetWorld())
    {
        NextAttackTime = World->GetTimeSeconds() + FMath::Max(0.1f, RecoverySeconds);
    }
    UpdateStateAppearance();
    AttackTarget.Reset();
}

bool ATrainingEnemy::IsDangerChallengeFor(const AParryActionCharacter* Player) const
{
    return bDangerChallengeActive && AttackTarget.Get() == Player;
}

void ATrainingEnemy::BeginDangerChallenge(AParryActionCharacter* Player)
{
    if (!IsValid(Player) || bDefeated || bDangerChallengeActive) return;
    bDangerChallengeActive = true;
    AttackTarget = Player;
    DangerChallengeSequence.Reset();
    DangerChallengeIndex = 0;
    constexpr TCHAR Keys[] = TEXT("WASD");
    for (int32 Index = 0; Index < 4; ++Index) DangerChallengeSequence.AppendChar(Keys[FMath::RandRange(0, 3)]);
    bWindingUp = false;
    StopUnblockableLunge();
    GetWorldTimerManager().ClearTimer(WindupTimer);
    GetWorldTimerManager().ClearTimer(AttackVisualStartTimer);
    GetWorldTimerManager().ClearTimer(AttackVisualEndTimer);
    if (EnemyVisualMesh) EnemyVisualMesh->bPauseAnims = true;
    Player->BeginDangerChallenge();
    UpdateStateAppearance();
    GetWorldTimerManager().SetTimer(DangerChallengeTimer, this, &ATrainingEnemy::DangerChallengeTimeout,
        FMath::Max(0.5f, DangerChallengeSeconds), false);
}

FString ATrainingEnemy::GetDangerChallengePrompt() const
{
    if (!bDangerChallengeActive) return FString();
    FString Text;
    for (int32 Index = 0; Index < DangerChallengeSequence.Len(); ++Index)
    {
        if (Index > 0) Text += TEXT("  ");
        Text += Index == DangerChallengeIndex ? FString::Printf(TEXT("[%c]"), DangerChallengeSequence[Index])
            : FString::Printf(TEXT("%c"), DangerChallengeSequence[Index]);
    }
    return FString::Printf(TEXT("%s    %d / 4"), *Text, DangerChallengeIndex);
}

float ATrainingEnemy::GetDangerChallengeFraction() const
{
    if (!bDangerChallengeActive || !GetWorld()) return 0.0f;
    return FMath::Clamp(GetWorldTimerManager().GetTimerRemaining(DangerChallengeTimer)
        / FMath::Max(0.5f, DangerChallengeSeconds), 0.0f, 1.0f);
}

void ATrainingEnemy::SubmitDangerChallengeKey(TCHAR Key)
{
    if (!bDangerChallengeActive || !DangerChallengeSequence.IsValidIndex(DangerChallengeIndex)) return;
    if (DangerChallengeSequence[DangerChallengeIndex] != Key)
    {
        FinishDangerChallenge(false);
        return;
    }
    if (++DangerChallengeIndex == DangerChallengeSequence.Len()) FinishDangerChallenge(true);
}

void ATrainingEnemy::DangerChallengeTimeout()
{
    FinishDangerChallenge(false);
}

void ATrainingEnemy::CancelDangerChallenge()
{
    if (!bDangerChallengeActive) return;
    bDangerChallengeActive = false;
    GetWorldTimerManager().ClearTimer(DangerChallengeTimer);
    if (EnemyVisualMesh) EnemyVisualMesh->bPauseAnims = false;
    if (AParryActionCharacter* Player = AttackTarget.Get()) Player->EndDangerChallenge();
    DangerChallengeSequence.Reset();
    DangerChallengeIndex = 0;
}

void ATrainingEnemy::FinishDangerChallenge(bool bSuccess)
{
    if (!bDangerChallengeActive) return;
    AParryActionCharacter* Player = AttackTarget.Get();
    CancelDangerChallenge();
    bLungeDirectionLocked = false;
    bAttackVisualPlaying = false;
    if (UWorld* World = GetWorld())
    {
        NextAttackTime = World->GetTimeSeconds() + FMath::Max(0.1f, RecoverySeconds);
        LastAttackOutcome = static_cast<int32>(bSuccess ? ETrainingAttackOutcome::Parried : ETrainingAttackOutcome::PlayerHit);
        AttackOutcomeEndTime = World->GetTimeSeconds() + 1.5f;
    }
    if (IsValid(Player) && !Player->IsDefeated() && !bDefeated)
    {
        if (bSuccess)
        {
            Player->RecordSuccessfulParry();
            if (USoundBase* Sound = ParrySuccessSoundAsset.LoadSynchronous()) UGameplayStatics::PlaySound2D(this, Sound, 0.5f);
            if (UNiagaraSystem* Effect = ParrySuccessEffectAsset.LoadSynchronous())
                UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Effect, (GetActorLocation() + Player->GetActorLocation()) * 0.5f + FVector(0, 0, 30));
            bStunned = true;
            StartParryStunVisual();
            GetWorldTimerManager().SetTimer(StunTimer, this, &ATrainingEnemy::EndParryStun,
                FMath::Max(0.1f, DangerParryStunSeconds), false);
        }
        else
        {
            // Twice the total two-strike damage, applied once; never queue strike two.
            UGameplayStatics::ApplyDamage(Player, FMath::Max(0.0f, UnblockableDamage * 2.0f), nullptr, this, UDamageType::StaticClass());
        }
    }
    if (!bStunned && EnemyVisualMesh && IdleAnimation) EnemyVisualMesh->PlayAnimation(IdleAnimation, true);
    AttackTarget.Reset();
    UpdateStateAppearance();
}

void ATrainingEnemy::EndParryStun()
{
    if (bDefeated)
    {
        return;
    }
    if (bStunVisualPlaying)
    {
        EndParryStunVisual();
    }
    bStunned = false;
    UpdateStateAppearance();
}
