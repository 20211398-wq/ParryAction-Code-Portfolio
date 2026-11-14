#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrainingEnemy.generated.h"

class AParryActionCharacter;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UAnimSequenceBase;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UNiagaraSystem;
class USoundBase;

enum class ETrainingAttackType : uint8
{
    Parryable,
    Unblockable
};

// Values match the existing HUD outcome codes.
enum class ETrainingAttackOutcome : uint8
{
    OutOfRange = 1,
    PlayerHit = 2,
    Parried = 3,
    Dodged = 4
};

/** Training enemy with simple flat-ground pursuit and telegraphed attacks. */
UCLASS(Blueprintable)
class PARRYACTION_API ATrainingEnemy : public AActor
{
    GENERATED_BODY()
public:
    ATrainingEnemy();
    /** Classify the hit without changing state; timing and reach are checked by the caller. */
    static ETrainingAttackOutcome ClassifyAttack(bool bInRange, ETrainingAttackType AttackType, bool bParryActive, bool bDodgeInvulnerable);
    /** Choose a random pattern, forcing a switch after two of the same type. */
    static ETrainingAttackType ChooseNextAttack(ETrainingAttackType Previous, int32 ConsecutiveCount, bool bPickUnblockable);
    UFUNCTION(BlueprintPure, Category="Enemy|Health")
    bool IsDefeated() const { return bDefeated; }
    float GetCurrentHealth() const { return CurrentHealth; }
    float GetMaxHealth() const { return MaxHealth; }
    bool IsStunned() const { return bStunned; }
    bool IsDangerChallengeActive() const { return bDangerChallengeActive; }
    bool IsDangerChallengeFor(const AParryActionCharacter* Player) const;
    void SubmitDangerChallengeKey(TCHAR Key);
    FString GetDangerChallengePrompt() const;
    float GetDangerChallengeFraction() const;
    void CancelDangerChallenge();
    /** Remaining portion of the actual parry stun timer, for presentation. */
    float GetParryStunFraction() const;
    bool IsWindingUp() const { return bWindingUp; }
    /** Fraction remaining until the existing windup timer resolves the attack. */
    float GetAttackWindupFraction() const;
    bool IsUnblockableAttack() const { return CurrentAttackType == ETrainingAttackType::Unblockable; }
    /** 0 outside a danger attack; otherwise the upcoming strike is 1 or 2. */
    int32 GetDangerStrikeNumber() const
    {
        return bWindingUp && !bDefeated && IsUnblockableAttack() ? UnblockableStrikeIndex + 1 : 0;
    }
    /** 0: none, 1: avoided by range, 2: player hit, 3: parried, 4: dodge invulnerability. */
    int32 GetRecentAttackOutcome() const;
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy")
    TObjectPtr<UStaticMeshComponent> EnemyMesh;

    /** Visual mesh is separate from the original collision root. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy|Visual")
    TObjectPtr<USkeletalMeshComponent> EnemyVisualMesh;

    UPROPERTY(EditDefaultsOnly, Category="Enemy|Visual")
    TObjectPtr<UAnimSequenceBase> IdleAnimation;

    UPROPERTY(EditDefaultsOnly, Category="Enemy|Visual")
    TObjectPtr<UAnimSequenceBase> WalkAnimation;

    /** Visual-only attack; damage remains controlled by the attack timer. */
    UPROPERTY(EditDefaultsOnly, Category="Enemy|Visual")
    TObjectPtr<UAnimSequenceBase> AttackVisualAnimation;

    /** Different visual for the danger attack. */
    UPROPERTY(EditDefaultsOnly, Category="Enemy|Visual")
    TObjectPtr<UAnimSequenceBase> UnblockableAttackVisualAnimation;

    /** Second punch uses a root-motion-free animation already used by the player. */
    UPROPERTY(EditDefaultsOnly, Category="Enemy|Visual")
    TObjectPtr<UAnimSequenceBase> UnblockableSecondVisualAnimation;

    /** Approximate strike point in the second punch animation. */
    UPROPERTY(EditDefaultsOnly, Category="Enemy|Visual", meta=(ClampMin="0.0"))
    float UnblockableSecondVisualHitTime = 0.5f;

    /** Approximate strike point in the unblockable visual animation. */
    UPROPERTY(EditDefaultsOnly, Category="Enemy|Visual", meta=(ClampMin="0.0"))
    float UnblockableVisualHitTime = 1.1f;

    /** Visual-only reaction after the player successfully parries. */
    UPROPERTY(EditDefaultsOnly, Category="Enemy|Visual")
    TObjectPtr<UAnimSequenceBase> ParryStunAnimation;

    /** Visual-only defeat animation; victory is decided immediately. */
    UPROPERTY(EditDefaultsOnly, Category="Enemy|Visual")
    TObjectPtr<UAnimSequenceBase> DefeatAnimation;

    /** Estimated point in the animation at which the hand reaches the target. */
    UPROPERTY(EditDefaultsOnly, Category="Enemy|Visual", meta=(ClampMin="0.0"))
    float AttackVisualHitTime = 0.35f;

    /** Material and colors for reading combat states on the prototype enemy. */
    UPROPERTY(EditDefaultsOnly, Category="Enemy|Visual")
    TObjectPtr<UMaterialInterface> StateMaterial;

    /** Additive overlay keeps character textures visible beneath combat cues. */
    UPROPERTY(EditDefaultsOnly, Category="Enemy|Visual")
    TObjectPtr<UMaterialInterface> WarningOverlayMaterial;

    UPROPERTY(EditDefaultsOnly, Category="Enemy|Visual")
    FLinearColor IdleColor = FLinearColor(0.22f, 0.22f, 0.22f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category="Enemy|Visual")
    FLinearColor ParryableColor = FLinearColor(1.0f, 0.65f, 0.02f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category="Enemy|Visual")
    FLinearColor UnblockableColor = FLinearColor(1.0f, 0.03f, 0.01f, 1.0f);

    UPROPERTY(EditDefaultsOnly, Category="Enemy|Visual")
    FLinearColor StunnedColor = FLinearColor(0.02f, 0.6f, 1.0f, 1.0f);

    /** Brief visual feedback when a nonlethal hit lands; does not affect AI or damage. */
    UPROPERTY(EditDefaultsOnly, Category="Enemy|Visual", meta=(ClampMin="0.0"))
    float HitFlashSeconds = 0.16f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy|Health", meta=(ClampMin="1.0"))
    float MaxHealth = 160.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Enemy|Health")
    float CurrentHealth = 160.0f;

    /** A short effect played between the player and enemy on a successful parry. */
    UPROPERTY(EditDefaultsOnly, Category="Enemy|Effects")
    TSoftObjectPtr<UNiagaraSystem> ParrySuccessEffectAsset;

    /** Feedback played only when the attack is successfully parried. */
    UPROPERTY(EditDefaultsOnly, Category="Enemy|Effects")
    TSoftObjectPtr<USoundBase> ParrySuccessSoundAsset;

    /** How long a successful parry stops the enemy from starting attacks. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy|Parry", meta=(ClampMin="0.1"))
    float ParryStunSeconds = 3.0f;

    /** A successful danger challenge gives a longer counter window. */
    UPROPERTY(EditDefaultsOnly, Category="Enemy|Parry", meta=(ClampMin="0.1"))
    float DangerParryStunSeconds = 4.5f;
    UPROPERTY(EditDefaultsOnly, Category="Enemy|Parry", meta=(ClampMin="0.5"))
    float DangerChallengeSeconds = 3.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Enemy|Parry")
    bool bStunned = false;

    /** Simple direct pursuit for the flat prototype arena; this is not navigation pathfinding. */
    UPROPERTY(EditDefaultsOnly, Category="Enemy|Movement", meta=(ClampMin="0.0"))
    float ChaseSpeed = 150.0f;

    UPROPERTY(EditDefaultsOnly, Category="Enemy|Movement", meta=(ClampMin="1.0"))
    float DetectionRange = 1200.0f;

    UPROPERTY(EditDefaultsOnly, Category="Enemy|Movement", meta=(ClampMin="1.0"))
    float ChaseStopDistance = 170.0f;

    UPROPERTY(EditDefaultsOnly, Category="Enemy|Attack", meta=(ClampMin="1.0"))
    float AttackRange = 200.0f;

    UPROPERTY(EditDefaultsOnly, Category="Enemy|Attack", meta=(ClampMin="0.1"))
    float WindupSeconds = 1.0f;

    UPROPERTY(EditDefaultsOnly, Category="Enemy|Attack", meta=(ClampMin="0.0"))
    float AttackDamage = 20.0f;

    /** Longer warning and higher damage for the danger attack. */
    UPROPERTY(EditDefaultsOnly, Category="Enemy|Attack", meta=(ClampMin="0.1"))
    float UnblockableWindupSeconds = 1.5f;

    UPROPERTY(EditDefaultsOnly, Category="Enemy|Attack", meta=(ClampMin="0.0"))
    float UnblockableDamage = 30.0f;

    /** Gap from the first hit to the second; total danger damage is split across both hits. */
    UPROPERTY(EditDefaultsOnly, Category="Enemy|Attack", meta=(ClampMin="0.1"))
    float UnblockableSecondHitDelay = 0.75f;

    UPROPERTY(EditDefaultsOnly, Category="Enemy|Attack", meta=(ClampMin="0.0"))
    float UnblockableLungeDistance = 120.0f;

    UPROPERTY(EditDefaultsOnly, Category="Enemy|Attack", meta=(ClampMin="0.05"))
    float UnblockableLungeDuration = 0.3f;

    UPROPERTY(EditDefaultsOnly, Category="Enemy|Attack", meta=(ClampMin="0.1"))
    float RecoverySeconds = 2.0f;

private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FTrainingEnemyDangerComboTest;
    friend class FParryActionAttackDamageTest;
    friend class FDangerComboLatentCommand;
    friend class FStrongParryChallengeTest;
    friend class FStrongParryTimerTest;
#endif
    void UpdateStateAppearance();
    void EndHitFlash();
    void UpdateVisualMovement();
    void StartAttackVisual();
    void EndAttackVisual();
    void StartParryStunVisual();
    void EndParryStunVisual();
    void HideDefeatedEnemy();
    FVector LastVisualLocation = FVector::ZeroVector;
    bool bVisualWalking = false;
    bool bAttackVisualPlaying = false;
    bool bStunVisualPlaying = false;
    bool bHitFlashActive = false;
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> StateMaterialInstance;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> WarningOverlayInstance;

    void UpdatePursuit();
    bool MoveOnSafeGround(const FVector& Direction, float Distance, const AParryActionCharacter* Player);
    void ScheduleUnblockableLunge(float TimeUntilHit);
    void BeginUnblockableLunge();
    void UpdateUnblockableLunge();
    void StopUnblockableLunge();
    void CheckForPlayer();
    void ResolveAttack();
    void EndParryStun();
    void BeginDangerChallenge(AParryActionCharacter* Player);
    void FinishDangerChallenge(bool bSuccess);
    void DangerChallengeTimeout();
    bool CanReach(const AParryActionCharacter* Character) const;
    TWeakObjectPtr<AParryActionCharacter> AttackTarget;
    FTimerHandle DetectionTimer;
    FTimerHandle PursuitTimer;
    FTimerHandle VisualTimer;
    FTimerHandle AttackVisualStartTimer;
    FTimerHandle AttackVisualEndTimer;
    FTimerHandle StunVisualEndTimer;
    FTimerHandle DefeatVisualEndTimer;
    FTimerHandle WindupTimer;
    FTimerHandle LungeStartTimer;
    FTimerHandle LungeMovementTimer;
    FTimerHandle StunTimer;
    FTimerHandle HitFlashTimer;
    FTimerHandle DangerChallengeTimer;
    FString DangerChallengeSequence;
    int32 DangerChallengeIndex = 0;
    bool bDangerChallengeActive = false;
    float NextAttackTime = 0.0f;
    float NextPursuitDiagnosticTime = 0.0f;
    float AttackOutcomeEndTime = -1.0f;
    int32 LastAttackOutcome = 0;
    ETrainingAttackType CurrentAttackType = ETrainingAttackType::Parryable;
    ETrainingAttackType NextAttackType = ETrainingAttackType::Parryable;
    int32 ConsecutiveAttackCount = 0;
    int32 UnblockableStrikeIndex = 0;
    FVector LockedLungeDirection = FVector::ZeroVector;
    float LungeDistanceRemaining = 0.0f;
    bool bLungeDirectionLocked = false;
    bool bWindingUp = false;
    bool bDefeated = false;
};
