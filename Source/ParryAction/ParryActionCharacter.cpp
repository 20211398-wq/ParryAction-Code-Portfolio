// Copyright Epic Games, Inc. All Rights Reserved.

#include "ParryActionCharacter.h"
#include "ParryActionPlayerController.h"
#include "PracticeTarget.h"
#include "TrainingEnemy.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Camera/CameraShakeBase.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "ParryAction.h"

AParryActionCharacter::AParryActionCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Use the template's existing unarmed animation; designers can replace it in the character Blueprint.
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> DefaultAttackAnimation(
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01"));
	if (DefaultAttackAnimation.Succeeded())
	{
		AttackAnimation = DefaultAttackAnimation.Object;
	}

	// Reuse the template's hit effect and camera shake for clear successful-hit feedback.
	// Resolve the effect only when needed, after Niagara module startup.
	AttackHitEffectAsset = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Variant_Combat/VFX/NS_Damage.NS_Damage")));
	AttackHitSoundAsset = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/ParryAction/Audio/SFX_AttackHit.SFX_AttackHit")));
	static ConstructorHelpers::FClassFinder<UCameraShakeBase> HitShake(TEXT("/Game/Variant_Combat/Blueprints/BP_CameraShake_Hit_Enemy"));
	if (HitShake.Succeeded())
	{
		AttackHitCameraShake = HitShake.Class;
	}

	static ConstructorHelpers::FClassFinder<UCameraShakeBase> DamageShake(TEXT("/Game/Variant_Combat/Blueprints/BP_CameraShake_Hit_Player"));
	if (DamageShake.Succeeded())
	{
		DamageCameraShake = DamageShake.Class;
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> DefeatVisual(
		TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Back_01.MM_Death_Back_01"));
	if (DefeatVisual.Succeeded())
	{
		DefeatAnimation = DefeatVisual.Object;
	}

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AParryActionCharacter::BeginPlay()
{
	Super::BeginPlay();
	MaxHealth = FMath::Max(1.0f, MaxHealth);
	CurrentHealth = MaxHealth;
	bDefeated = false;
	bCombatWon = false;
	bDodgeInvulnerable = false;
	SuccessfulParryCount = 0;
	TotalDamageTaken = 0.0f;
	DefeatElapsedSeconds = -1.0f;
	if (const UWorld* World = GetWorld())
	{
		CombatStartTime = World->GetTimeSeconds();
	}
}

void AParryActionCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearQueuedSecondAttack();
	bAttackHitPending = false;
	ActiveAttackMontage = nullptr;
	ActiveAttackAnimation = nullptr;
	bComboAvailable = false;
	ActiveParryMontage = nullptr;
	bDodgeInvulnerable = false;
	GetWorldTimerManager().ClearTimer(DodgeInvulnerabilityTimer);
	GetWorldTimerManager().ClearTimer(ParryWindowTimer);
	Super::EndPlay(EndPlayReason);
}



float AParryActionCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bDefeated || bCombatWon || bDodgeInvulnerable || DamageAmount <= 0.0f)
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
	TotalDamageTaken += PreviousHealth - CurrentHealth;
	if (CurrentHealth < PreviousHealth && DamageCameraShake && DamageCameraShakeScale > 0.0f)
	{
		if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
		{
			PlayerController->ClientStartCameraShake(DamageCameraShake, DamageCameraShakeScale);
		}
	}
	if (CurrentHealth <= 0.0f)
	{
		// Capture the result before the HUD gets its next update.
		DefeatElapsedSeconds = GetCombatElapsedSeconds();
		bDefeated = true;
		ClearQueuedSecondAttack();
		bDodgeInvulnerable = false;
		GetWorldTimerManager().ClearTimer(DodgeInvulnerabilityTimer);
		bAttackHitPending = false;
		if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
		{
			if (ActiveAttackMontage)
			{
				AnimInstance->Montage_Stop(0.1f, ActiveAttackMontage);
			}
		}
		ActiveAttackMontage = nullptr;
		ActiveAttackAnimation = nullptr;
		bComboAvailable = false;
		ActiveParryMontage = nullptr;
		bParryActive = false;
		GetWorldTimerManager().ClearTimer(ParryWindowTimer);
		StopMovementForResult();
		// The gameplay result is immediate; only the visual mesh plays this once.
		if (USkeletalMeshComponent* CharacterMesh = GetMesh(); CharacterMesh && DefeatAnimation)
		{
			CharacterMesh->PlayAnimation(DefeatAnimation, false);
		}
		if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
		{
			if (AParryActionPlayerController* ParryController = Cast<AParryActionPlayerController>(PlayerController))
			{
				ParryController->StopCombatMusic();
			}
			DisableInput(PlayerController);
		}
	}
	return PreviousHealth - CurrentHealth;
}

void AParryActionCharacter::BeginDangerChallenge()
{
    bDangerChallengeLocked = true;
    CurrentMoveInput = FVector2D::ZeroVector;
    ClearQueuedSecondAttack();
    bComboAvailable = false;
    bAttackHitPending = false;
    if (UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
    {
        if (ActiveAttackMontage) Anim->Montage_Stop(0.1f, ActiveAttackMontage);
    }
    ActiveAttackMontage = nullptr;
    ActiveAttackAnimation = nullptr;
    GetWorldTimerManager().ClearTimer(ParryWindowTimer);
    EndParry();
    bDodgeInvulnerable = false;
    GetWorldTimerManager().ClearTimer(DodgeInvulnerabilityTimer);
    StopJumping();
    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->StopMovementImmediately();
        Movement->ClearAccumulatedForces();
    }
}

void AParryActionCharacter::EndDangerChallenge()
{
    bDangerChallengeLocked = false;
    CurrentMoveInput = FVector2D::ZeroVector;
}

void AParryActionCharacter::StopMovementForResult()
{
	StopJumping();
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		const bool bWasFalling = Movement->IsFalling();
		Movement->StopMovementImmediately();
		// LaunchCharacter is deferred, so clear a dodge launch that has not yet been applied.
		Movement->ClearAccumulatedForces();
		if (!bWasFalling)
		{
			Movement->DisableMovement();
		}
		// Falling remains enabled so gravity can bring the character to the floor.
	}
}

void AParryActionCharacter::HandleVictory()
{
	if (bDefeated || bCombatWon)
	{
		return;
	}
	bCombatWon = true;
	ClearQueuedSecondAttack();
	bComboAvailable = false;
	bAttackHitPending = false;
	bDodgeInvulnerable = false;
	GetWorldTimerManager().ClearTimer(DodgeInvulnerabilityTimer);
	GetWorldTimerManager().ClearTimer(ParryWindowTimer);
	EndParry();
	CurrentMoveInput = FVector2D::ZeroVector;
	StopMovementForResult();
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		// Character actions stop; R and T remain bound to the player controller.
		DisableInput(PlayerController);
	}
}

float AParryActionCharacter::GetCombatElapsedSeconds() const
{
	if (DefeatElapsedSeconds >= 0.0f)
	{
		return DefeatElapsedSeconds;
	}
	const UWorld* World = GetWorld();
	return World ? FMath::Max(0.0f, World->GetTimeSeconds() - CombatStartTime) : 0.0f;
}

void AParryActionCharacter::RecordSuccessfulParry()
{
	if (!bDefeated)
	{
		++SuccessfulParryCount;
	}
}

void AParryActionCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AParryActionCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AParryActionCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AParryActionCharacter::Look);

		// Clear the cached input when movement stops or is cancelled.
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &AParryActionCharacter::StopMoving);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Canceled, this, &AParryActionCharacter::StopMoving);

		if (AttackAction)
		{
			EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &AParryActionCharacter::Attack);
		}
		else
		{
			UE_LOG(LogParryAction, Warning, TEXT("AttackAction is not assigned on %s."), *GetNameSafe(this));
		}

		if (ParryAction)
		{
			EnhancedInputComponent->BindAction(ParryAction, ETriggerEvent::Started, this, &AParryActionCharacter::StartParry);
		}
		else
		{
			UE_LOG(LogParryAction, Warning, TEXT("ParryAction is not assigned on %s."), *GetNameSafe(this));
		}

		if (DodgeAction)
		{
			EnhancedInputComponent->BindAction(DodgeAction, ETriggerEvent::Started, this, &AParryActionCharacter::Dodge);
		}
		else
		{
			UE_LOG(LogParryAction, Warning, TEXT("DodgeAction is not assigned on %s."), *GetNameSafe(this));
		}

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AParryActionCharacter::Look);
	}
	else
	{
		UE_LOG(LogParryAction, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AParryActionCharacter::Move(const FInputActionValue& Value)
{
    if (bDangerChallengeLocked) { CurrentMoveInput = FVector2D::ZeroVector; return; }
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();
	CurrentMoveInput = MovementVector;

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void AParryActionCharacter::StopMoving(const FInputActionValue& Value)
{
	CurrentMoveInput = FVector2D::ZeroVector;
}

void AParryActionCharacter::Attack()
{
	UWorld* World = GetWorld();
	if (!World || bDefeated || bCombatWon || bDangerChallengeLocked)
	{
		return;
	}
	if (bAttackHitPending || World->GetTimeSeconds() < NextAttackTime)
	{
		// Remember at most one early second input from the first attack.
		if (bComboAvailable && ActiveAttackMontage && ActiveAttackAnimation == AttackAnimation
			&& World->GetTimeSeconds() <= ComboWindowEndTime)
		{
			bSecondAttackQueued = true;
			TryExecuteQueuedSecondAttack();
		}
		return;
	}
	// A fresh eligible input consumes any queued input and must not start two attacks.
	ClearQueuedSecondAttack();
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AttackAnimation || !AnimInstance)
	{
		UE_LOG(LogParryAction, Warning, TEXT("Cannot attack: animation or animation instance is missing."));
		return;
	}
	const bool bSecondAttack = bComboAvailable && World->GetTimeSeconds() <= ComboWindowEndTime && SecondAttackAnimation;
	UAnimSequenceBase* SelectedAnimation = bSecondAttack ? SecondAttackAnimation.Get() : AttackAnimation.Get();
	UAnimMontage* Montage = AnimInstance->PlaySlotAnimationAsDynamicMontage(SelectedAnimation, TEXT("DefaultSlot"), 0.1f, 0.15f);
	if (!Montage)
	{
		UE_LOG(LogParryAction, Warning, TEXT("Cannot attack: failed to start the attack montage."));
		return;
	}
	ActiveAttackMontage = Montage;
	ActiveAttackAnimation = SelectedAnimation;
	bComboAvailable = !bSecondAttack && SecondAttackAnimation && ComboWindowSeconds > 0.0f;
	ComboWindowEndTime = World->GetTimeSeconds() + FMath::Max(0.0f, ComboWindowSeconds);
	bAttackHitPending = true;
	AttackFeedbackEndTime = -1.0f;
	NextAttackTime = World->GetTimeSeconds() + FMath::Max(0.0f, AttackCooldown);
	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &AParryActionCharacter::OnAttackMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);
}

void AParryActionCharacter::ClearQueuedSecondAttack()
{
	bSecondAttackQueued = false;
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(ComboBufferTimer);
	}
}

void AParryActionCharacter::TryExecuteQueuedSecondAttack()
{
	if (!bSecondAttackQueued)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World || bDefeated || !bComboAvailable || World->GetTimeSeconds() > ComboWindowEndTime)
	{
		ClearQueuedSecondAttack();
		return;
	}
	// Wait for the first hit notify so it cannot be replaced by the next montage.
	if (bAttackHitPending)
	{
		return;
	}
	const float RemainingCooldown = NextAttackTime - World->GetTimeSeconds();
	if (RemainingCooldown > 0.0f)
	{
		GetWorldTimerManager().SetTimer(ComboBufferTimer, this,
			&AParryActionCharacter::TryExecuteQueuedSecondAttack, RemainingCooldown, false);
		return;
	}
	ClearQueuedSecondAttack();
	Attack();
}

void AParryActionCharacter::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage == ActiveAttackMontage)
	{
		// A missing notify or an interrupted first attack must not release a stale input.
		if (bInterrupted || bAttackHitPending)
		{
			ClearQueuedSecondAttack();
			bComboAvailable = false;
		}
		bAttackHitPending = false;
		ActiveAttackMontage = nullptr;
		ActiveAttackAnimation = nullptr;
		if (bInterrupted)
		{
			bComboAvailable = false;
		}
	}
}

void AParryActionCharacter::HandleAttackHitNotify(UAnimSequenceBase* Animation)
{
	if (!bAttackHitPending || bDefeated || !ActiveAttackMontage || Animation != ActiveAttackAnimation)
	{
		return;
	}
	bAttackHitPending = false;
	ResolveAttack();
	TryExecuteQueuedSecondAttack();
}

void AParryActionCharacter::ResolveAttack()
{
	UWorld* World = GetWorld();
	if (!World || bDefeated)
	{
		return;
	}
	const FVector Start = GetActorLocation();
	const FVector End = Start + GetActorForwardVector() * FMath::Max(0.0f, AttackRange);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BasicAttack), false, this);
	FHitResult Hit;
	const bool bHit = World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity,
		ECC_Visibility, FCollisionShape::MakeSphere(FMath::Max(1.0f, AttackRadius)), Params);
	APracticeTarget* Target = bHit ? Cast<APracticeTarget>(Hit.GetActor()) : nullptr;
	ATrainingEnemy* Enemy = bHit ? Cast<ATrainingEnemy>(Hit.GetActor()) : nullptr;
	bool bSuccessfulHit = false;
	bool bWasCounter = false;
	if (Target)
	{
		Target->RegisterPracticeHit();
		bSuccessfulHit = true;
	}
	else if (Enemy)
	{
		// Capture the state before damage: the final hit may clear stun when it defeats the enemy.
		bWasCounter = Enemy->IsStunned();
		const float Multiplier = bWasCounter ? FMath::Max(1.0f, CounterDamageMultiplier) : 1.0f;
		const float Damage = FMath::Max(0.0f, PlayerAttackDamage) * Multiplier;
		bSuccessfulHit = UGameplayStatics::ApplyDamage(Enemy, Damage, GetController(), this, UDamageType::StaticClass()) > 0.0f;
		if (Enemy->IsDefeated())
		{
			ClearQueuedSecondAttack();
			bComboAvailable = false;
		}
	}
	if (bSuccessfulHit)
	{
		// Only an actual hit against a stunned enemy receives stronger feedback.
		const float FeedbackScale = bWasCounter ? FMath::Clamp(CounterFeedbackMultiplier, 1.0f, 2.0f) : 1.0f;
		if (USoundBase* HitSound = AttackHitSoundAsset.LoadSynchronous())
		{
			UGameplayStatics::PlaySound2D(this, HitSound, 0.55f);
		}
		if (UNiagaraSystem* HitEffect = AttackHitEffectAsset.LoadSynchronous())
		{
			const FVector EffectLocation = Hit.ImpactPoint + Hit.ImpactNormal * 3.0f;
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, HitEffect, EffectLocation,
				FRotator::ZeroRotator, FVector(FeedbackScale));
		}
		if (AttackHitCameraShake && AttackHitShakeScale > 0.0f)
		{
			if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
			{
				PlayerController->ClientStartCameraShake(AttackHitCameraShake, AttackHitShakeScale * FeedbackScale);
			}
		}
	}
	bLastAttackHit = bSuccessfulHit;
	bLastAttackCounter = bSuccessfulHit && bWasCounter;
	AttackFeedbackEndTime = World->GetTimeSeconds() + 0.7f;
}

bool AParryActionCharacter::HasRecentAttackResult() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < AttackFeedbackEndTime;
}

void AParryActionCharacter::StartParry()
{
	if (bDefeated || bCombatWon || bParryActive || bDangerChallengeLocked || !GetWorld())
	{
		return;
	}

	bParryActive = true;
	const float Duration = FMath::Max(0.05f, ParryWindowDuration);
	GetWorldTimerManager().SetTimer(ParryWindowTimer, this, &AParryActionCharacter::EndParry, Duration, false);

	// This is cosmetic: never interrupt an attack or dodge montage for a parry pose.
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (AnimInstance && ParryVisualAnimation && !AnimInstance->IsAnyMontagePlaying())
	{
		ActiveParryMontage = AnimInstance->PlaySlotAnimationAsDynamicMontage(
			ParryVisualAnimation, TEXT("DefaultSlot"), 0.05f, 0.1f);
	}
}

float AParryActionCharacter::GetParryWindowFraction() const
{
	const UWorld* World = GetWorld();
	if (!World || !bParryActive)
	{
		return 0.0f;
	}
	const float Duration = FMath::Max(0.05f, ParryWindowDuration);
	return FMath::Clamp(World->GetTimerManager().GetTimerRemaining(ParryWindowTimer) / Duration, 0.0f, 1.0f);
}

void AParryActionCharacter::EndParry()
{
	bParryActive = false;
	if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		// An attack or dodge may have replaced this montage while the window was open.
		if (ActiveParryMontage && AnimInstance->Montage_IsPlaying(ActiveParryMontage))
		{
			AnimInstance->Montage_Stop(0.1f, ActiveParryMontage);
		}
	}
	ActiveParryMontage = nullptr;
}

bool AParryActionCharacter::IsDodgeReady() const
{
	const UWorld* World = GetWorld();
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	return World && !bDefeated && !bCombatWon && !bDangerChallengeLocked && Movement && Controller
		&& Movement->IsMovingOnGround() && World->GetTimeSeconds() >= NextDodgeTime;
}

float AParryActionCharacter::GetDodgeCooldownFraction() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}
	const float Cooldown = FMath::Max(0.0f, DodgeCooldown);
	return Cooldown <= KINDA_SMALL_NUMBER ? 1.0f
		: 1.0f - FMath::Clamp((NextDodgeTime - World->GetTimeSeconds()) / Cooldown, 0.0f, 1.0f);
}

void AParryActionCharacter::Dodge()
{
	UWorld* World = GetWorld();
	if (!IsDodgeReady())
	{
		return;
	}

	FVector Direction = GetActorForwardVector();
	if (!CurrentMoveInput.IsNearlyZero())
	{
		const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
		Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X) * CurrentMoveInput.Y
			+ FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y) * CurrentMoveInput.X;
	}
	Direction.Z = 0.0f;
	Direction = Direction.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return;
	}

	// A dodge starts a new sequence, even when an active hit animation must finish.
	ClearQueuedSecondAttack();
	bComboAvailable = false;
	NextDodgeTime = World->GetTimeSeconds() + FMath::Max(0.0f, DodgeCooldown);
	LaunchCharacter(Direction * FMath::Max(0.0f, DodgeSpeed), true, false);
	const float InvulnerabilityDuration = FMath::Max(0.0f, DodgeInvulnerabilityDuration);
	if (InvulnerabilityDuration > 0.0f)
	{
		bDodgeInvulnerable = true;
		GetWorldTimerManager().SetTimer(DodgeInvulnerabilityTimer, this,
			&AParryActionCharacter::EndDodgeInvulnerability, InvulnerabilityDuration, false);
	}

	// During target lock the character keeps facing the opponent. Select a visual
	// using the actual launch direction relative to that facing; unlocked movement
	// may rotate the character, so it keeps the existing generic animation.
	UAnimSequenceBase* DodgeAnimation = DodgeVisualAnimation.Get();
	if (bUseControllerRotationYaw)
	{
		const float ForwardAmount = FVector::DotProduct(Direction, GetActorForwardVector());
		const float RightAmount = FVector::DotProduct(Direction, GetActorRightVector());
		UAnimSequenceBase* DirectionalAnimation = nullptr;
		if (FMath::Abs(RightAmount) > FMath::Abs(ForwardAmount))
		{
			DirectionalAnimation = RightAmount > 0.0f ? DodgeRightAnimation.Get() : DodgeLeftAnimation.Get();
		}
		else if (ForwardAmount < 0.0f)
		{
			DirectionalAnimation = DodgeBackAnimation.Get();
		}
		if (DirectionalAnimation)
		{
			DodgeAnimation = DirectionalAnimation;
		}
	}

	// Preserve an active attack and its hit notify; dodge movement still happens.
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	const bool bAttackMontagePlaying = AnimInstance && ActiveAttackMontage && AnimInstance->Montage_IsPlaying(ActiveAttackMontage);
	if (AnimInstance && DodgeAnimation && !bAttackHitPending && !bAttackMontagePlaying)
	{
		AnimInstance->PlaySlotAnimationAsDynamicMontage(DodgeAnimation, TEXT("DefaultSlot"), 0.1f, 0.15f);
	}
}

void AParryActionCharacter::EndDodgeInvulnerability()
{
	bDodgeInvulnerable = false;
}

void AParryActionCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AParryActionCharacter::DoMove(float Right, float Forward)
{
	if (!bDangerChallengeLocked && GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AParryActionCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AParryActionCharacter::DoJumpStart()
{
	// Keep the challenge input separate from ordinary movement.
	if (!bDangerChallengeLocked) Jump();
}

void AParryActionCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}
