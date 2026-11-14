// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "ParryActionCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UAnimSequenceBase;
class UAnimMontage;
class UNiagaraSystem;
class UCameraShakeBase;
class USoundBase;
class ATrainingEnemy;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class AParryActionCharacter : public ACharacter
{
	GENERATED_BODY()

#if WITH_DEV_AUTOMATION_TESTS
	friend class FParryActionDodgeReadinessTest;
	friend class FParryActionAttackDamageTest;
	friend class FParryActionComboBufferTest;
	friend class FComboBufferLatentCommand;
	friend class FParryActionAnimationHitTest;
	friend class FAnimationHitLatentCommand;
	friend class FStrongParryChallengeTest;
#endif

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Player health configuration. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Health", meta=(ClampMin="1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Health")
	float CurrentHealth = 100.0f;

	bool bDefeated = false;
	bool bCombatWon = false;
	bool bDangerChallengeLocked = false;

	/** Stop combat movement while allowing a character already in the air to land. */
	void StopMovementForResult();

	// Session statistics reset when a fresh character is spawned on restart.
	int32 SuccessfulParryCount = 0;
	float TotalDamageTaken = 0.0f;
	float CombatStartTime = 0.0f;
	float DefeatElapsedSeconds = -1.0f;

	/** Visual-only animation when player health reaches zero. */
	UPROPERTY(EditDefaultsOnly, Category="Health|Animation")
	TObjectPtr<UAnimSequenceBase> DefeatAnimation;

	/** Camera feedback for actual health loss. */
	UPROPERTY(EditDefaultsOnly, Category="Health|Feedback")
	TSubclassOf<UCameraShakeBase> DamageCameraShake;

	UPROPERTY(EditDefaultsOnly, Category="Health|Feedback", meta=(ClampMin="0.0"))
	float DamageCameraShakeScale = 0.4f;

	/** Action mapped to the basic attack button. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* AttackAction = nullptr;

	/** Animation used for the visual part of the basic attack. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Animation")
	TObjectPtr<UAnimSequenceBase> AttackAnimation;

	/** Optional second visual in the timed two-hit sequence. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Animation")
	TObjectPtr<UAnimSequenceBase> SecondAttackAnimation = nullptr;

	/** Feedback used only when an attack really hits the enemy or practice target. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Feedback")
	TSoftObjectPtr<UNiagaraSystem> AttackHitEffectAsset;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Feedback")
	TSoftObjectPtr<USoundBase> AttackHitSoundAsset;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Feedback")
	TSubclassOf<UCameraShakeBase> AttackHitCameraShake;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Feedback", meta=(ClampMin="0.0"))
	float AttackHitShakeScale = 0.25f;

	/** Visual and camera intensity multiplier for an actual counter hit. */
	UPROPERTY(EditDefaultsOnly, Category="Attack|Feedback", meta=(ClampMin="1.0", ClampMax="2.0"))
	float CounterFeedbackMultiplier = 1.5f;

	/** Distance from the character centre to the end of the attack sweep, in cm. */
	UPROPERTY(EditDefaultsOnly, Category="Attack", meta=(ClampMin="0.0"))
	float AttackRange = 190.0f;

	/** Radius of the attack sweep, in cm. */
	UPROPERTY(EditDefaultsOnly, Category="Attack", meta=(ClampMin="1.0"))
	float AttackRadius = 35.0f;

	/** Damage dealt to the training enemy by one attack. */
	UPROPERTY(EditDefaultsOnly, Category="Attack", meta=(ClampMin="0.0"))
	float PlayerAttackDamage = 10.0f;

	/** Damage multiplier while the enemy is stunned by a successful parry. */
	UPROPERTY(EditDefaultsOnly, Category="Attack", meta=(ClampMin="1.0"))
	float CounterDamageMultiplier = 2.0f;

	/** Minimum time between attacks, in seconds. */
	UPROPERTY(EditDefaultsOnly, Category="Attack", meta=(ClampMin="0.0"))
	float AttackCooldown = 0.5f;

	/** Time from the first attack in which a second eligible input can follow it. */
	UPROPERTY(EditDefaultsOnly, Category="Attack", meta=(ClampMin="0.0"))
	float ComboWindowSeconds = 1.4f;

	bool bAttackHitPending = false;
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveAttackMontage = nullptr;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> ActiveAttackAnimation = nullptr;
	bool bComboAvailable = false;
	bool bSecondAttackQueued = false;
	float ComboWindowEndTime = 0.0f;
	FTimerHandle ComboBufferTimer;
	void TryExecuteQueuedSecondAttack();
	void ClearQueuedSecondAttack();
	void ResolveAttack();
	void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	float NextAttackTime = 0.0f;
	float AttackFeedbackEndTime = -1.0f;
	bool bLastAttackHit = false;
	bool bLastAttackCounter = false;

	/** Action mapped to the parry button in the character blueprint. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ParryAction = nullptr;

	/** Duration of the parry window in seconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Parry", meta=(ClampMin="0.05"))
	float ParryWindowDuration = 0.3f;

	/** True while incoming attacks can later be checked for a parry. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Parry")
	bool bParryActive = false;

	/** Optional visual only; the parry timing remains controlled by ParryWindowDuration. */
	UPROPERTY(EditDefaultsOnly, Category="Parry|Animation")
	TObjectPtr<UAnimSequenceBase> ParryVisualAnimation = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveParryMontage = nullptr;

	FTimerHandle ParryWindowTimer;

	/** Action mapped to the dodge button in the character blueprint. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* DodgeAction = nullptr;

	/** Visual-only dodge animation, with root movement disabled on the asset. */
	UPROPERTY(EditDefaultsOnly, Category="Dodge|Animation")
	TObjectPtr<UAnimSequenceBase> DodgeVisualAnimation;

	/** Directional visuals for target lock; movement and invulnerability remain unchanged. */
	UPROPERTY(EditDefaultsOnly, Category="Dodge|Animation")
	TObjectPtr<UAnimSequenceBase> DodgeLeftAnimation = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Dodge|Animation")
	TObjectPtr<UAnimSequenceBase> DodgeRightAnimation = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Dodge|Animation")
	TObjectPtr<UAnimSequenceBase> DodgeBackAnimation = nullptr;

	/** Horizontal launch speed in cm/s. */
	UPROPERTY(EditAnywhere, Category="Dodge", meta=(ClampMin="0.0"))
	float DodgeSpeed = 1100.0f;

	/** Minimum time between dodges, in seconds. */
	UPROPERTY(EditAnywhere, Category="Dodge", meta=(ClampMin="0.0"))
	float DodgeCooldown = 0.7f;

	/** Brief protection starts when an eligible dodge begins. */
	UPROPERTY(EditDefaultsOnly, Category="Dodge", meta=(ClampMin="0.0"))
	float DodgeInvulnerabilityDuration = 0.25f;

	bool bDodgeInvulnerable = false;
	FTimerHandle DodgeInvulnerabilityTimer;
	void EndDodgeInvulnerability();

	FVector2D CurrentMoveInput = FVector2D::ZeroVector;
	float NextDodgeTime = 0.0f;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

public:

	/** Constructor */
	AParryActionCharacter();

	/** Finish combat without disabling the controller's result-screen inputs. */
	void HandleVictory();
    void BeginDangerChallenge();
    void EndDangerChallenge();
    bool IsDangerChallengeLocked() const { return bDangerChallengeLocked; }

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category="Health")
	bool IsDefeated() const { return bDefeated; }
	bool IsDodgeInvulnerable() const { return bDodgeInvulnerable; }
	/** Read-only presentation helpers; the cooldown still controls the actual dodge. */
	bool IsDodgeReady() const;
	float GetDodgeCooldownFraction() const;
	float GetCurrentHealth() const { return CurrentHealth; }
	float GetMaxHealth() const { return MaxHealth; }
	int32 GetSuccessfulParryCount() const { return SuccessfulParryCount; }
	float GetTotalDamageTaken() const { return TotalDamageTaken; }
	float GetCombatElapsedSeconds() const;
	void RecordSuccessfulParry();
	bool HasRecentAttackResult() const;
	bool DidLastAttackHit() const { return bLastAttackHit; }
	bool DidLastAttackCounter() const { return bLastAttackCounter; }	

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Clear direction when movement input is released. */
	void StopMoving(const FInputActionValue& Value);

	/** Sweep the space in front of the character for a practice target. */
	void Attack();

	/** Open and close the temporary parry window. */
	void StartParry();
	void EndParry();

	/** Attempt a grounded directional dodge. */
	void Dodge();

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

public:

	/** Called by the attack animation's notify at the hit frame. */
	void HandleAttackHitNotify(UAnimSequenceBase* Animation);

	/** Whether the current temporary parry window is open. */
	UFUNCTION(BlueprintPure, Category="Parry")
	bool IsParryActive() const { return bParryActive; }

	/** Remaining share of the active parry window, for UI display only. */
	float GetParryWindowFraction() const;

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};

