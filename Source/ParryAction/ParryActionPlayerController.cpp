// Copyright Epic Games, Inc. All Rights Reserved.


#include "ParryActionPlayerController.h"
#include "CombatHUDWidget.h"
#include "ParryActionCharacter.h"
#include "ParryActionSaveGame.h"
#include "TrainingEnemy.h"
#include "EnhancedInputComponent.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/InputComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "ParryAction.h"
#include "Widgets/Input/SVirtualJoystick.h"

namespace
{
    // Combat rules changed: preserve records earned under earlier rules.
    const TCHAR* const VictoryRecordSlot = TEXT("ParryAction_BestVictory_v4");
    constexpr int32 VictoryRecordUserIndex = 0;
}

AParryActionPlayerController::AParryActionPlayerController()
{
	CombatMusicAsset = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/ParryAction/Audio/Music_Combat.Music_Combat")));
}

void AParryActionPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalPlayerController())
	{
		TSubclassOf<UCombatHUDWidget> WidgetClass = CombatHUDClass;
		if (!WidgetClass)
		{
			WidgetClass = UCombatHUDWidget::StaticClass();
		}
		CombatHUD = CreateWidget<UCombatHUDWidget>(this, WidgetClass);
		if (CombatHUD)
		{
			CombatHUD->AddToViewport();
		}
		if (USoundBase* CombatMusic = CombatMusicAsset.LoadSynchronous())
		{
			CombatMusicComponent = UGameplayStatics::CreateSound2D(this, CombatMusic, FMath::Clamp(CombatMusicVolume, 0.0f, 1.0f), 1.0f, 0.0f, nullptr, false, false);
			if (CombatMusicComponent)
			{
				// Unlike UI sounds, music should pause with the game.
				CombatMusicComponent->bIsUISound = false;
				CombatMusicComponent->Play();
			}
		}

		const AGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr;
		const bool bSkipStart = GameMode && UGameplayStatics::ParseOption(GameMode->OptionsString, TEXT("SkipIntro")) == TEXT("1");
		if (CombatHUD && !bSkipStart)
		{
			// Read only for display; the victory path independently validates before writing.
			int32 StartBest = -1;
			bool bStartRecordUnavailable = false;
			if (UGameplayStatics::DoesSaveGameExist(VictoryRecordSlot, VictoryRecordUserIndex))
			{
				const UParryActionSaveGame* Record = Cast<UParryActionSaveGame>(
					UGameplayStatics::LoadGameFromSlot(VictoryRecordSlot, VictoryRecordUserIndex));
				if (Record && Record->BestVictoryCentiseconds >= -1)
				{
					StartBest = Record->BestVictoryCentiseconds;
				}
				else
				{
					bStartRecordUnavailable = true;
				}
			}
			CombatHUD->SetStartBestRecord(StartBest, bStartRecordUnavailable);
		}
		if (CombatHUD && !bSkipStart && SetPause(true))
		{
			bWaitingForStart = true;
			CombatHUD->SetStartMenuVisible(true);
		}
	}

	// only spawn touch controls on local player controllers
	if (ShouldUseTouchControls() && IsLocalPlayerController())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogParryAction, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void AParryActionPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearTargetLock();
	if (CombatMusicComponent)
	{
		CombatMusicComponent->Stop();
		CombatMusicComponent->DestroyComponent();
		CombatMusicComponent = nullptr;
	}
	if (CombatHUD)
	{
		CombatHUD->RemoveFromParent();
		CombatHUD = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AParryActionPlayerController::StopCombatMusic()
{
	if (CombatMusicComponent && CombatMusicComponent->IsPlaying())
	{
		CombatMusicComponent->FadeOut(0.4f, 0.0f);
	}
}

void AParryActionPlayerController::RecordVictory()
{
	if (bVictoryRecorded || !IsLocalPlayerController())
	{
		return;
	}
	const AParryActionCharacter* PlayerCharacter = Cast<AParryActionCharacter>(GetPawn());
	if (!IsValid(PlayerCharacter) || PlayerCharacter->IsDefeated())
	{
		return;
	}
	bVictoryRecorded = true;
	VictoryCentiseconds = FMath::Max(0, FMath::FloorToInt(PlayerCharacter->GetCombatElapsedSeconds() * 100.0f));
	const FString SlotName(VictoryRecordSlot);
	constexpr int32 UserIndex = VictoryRecordUserIndex;
	UParryActionSaveGame* Record = nullptr;
	if (UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
	{
		Record = Cast<UParryActionSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
		if (!Record || Record->BestVictoryCentiseconds < -1)
		{
			// Never overwrite an existing record that could not be read safely.
			bVictoryRecordUnavailable = true;
			UE_LOG(LogParryAction, Warning, TEXT("Could not load the victory record; existing save left untouched."));
			return;
		}
	}
	else
	{
		Record = Cast<UParryActionSaveGame>(UGameplayStatics::CreateSaveGameObject(UParryActionSaveGame::StaticClass()));
		if (!Record)
		{
			bVictoryRecordUnavailable = true;
			return;
		}
	}
	BestVictoryCentiseconds = Record->BestVictoryCentiseconds;
	if (Record->TryUpdateBestVictory(VictoryCentiseconds))
	{
		if (UGameplayStatics::SaveGameToSlot(Record, SlotName, UserIndex))
		{
			BestVictoryCentiseconds = VictoryCentiseconds;
			bNewVictoryRecord = true;
		}
		else
		{
			bVictoryRecordUnavailable = true;
			UE_LOG(LogParryAction, Warning, TEXT("Could not save the victory record."));
		}
	}
}

void AParryActionPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// These keys must remain responsive while the game is paused.
	InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AParryActionPlayerController::StartCombat).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &AParryActionPlayerController::ToggleTargetLock);
	InputComponent->BindKey(EKeys::T, IE_Pressed, this, &AParryActionPlayerController::ReturnToStartAfterResult);
	InputComponent->BindKey(EKeys::P, IE_Pressed, this, &AParryActionPlayerController::TogglePause).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::H, IE_Pressed, this, &AParryActionPlayerController::ToggleHelp).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AParryActionPlayerController::QuitFromPause).bExecuteWhenPaused = true;

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (RestartAction)
		{
			EnhancedInput->BindAction(RestartAction, ETriggerEvent::Started, this, &AParryActionPlayerController::RestartAfterResult);
		}
		else
		{
			UE_LOG(LogParryAction, Warning, TEXT("RestartAction is not assigned on %s."), *GetNameSafe(this));
		}
	}

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

bool AParryActionPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
    if (GetWorld() && !UGameplayStatics::IsGamePaused(this) && Params.Event == IE_Pressed)
    {
        AParryActionCharacter* ChallengePlayer = Cast<AParryActionCharacter>(GetPawn());
        ATrainingEnemy* Enemy = ChallengePlayer && ChallengePlayer->IsDangerChallengeLocked()
            ? Cast<ATrainingEnemy>(UGameplayStatics::GetActorOfClass(this, ATrainingEnemy::StaticClass())) : nullptr;
        if (Enemy && Enemy->IsDangerChallengeFor(ChallengePlayer))
        {
            TCHAR Letter = 0;
            if (Params.Key == EKeys::W) Letter = TEXT('W');
            else if (Params.Key == EKeys::A) Letter = TEXT('A');
            else if (Params.Key == EKeys::S) Letter = TEXT('S');
            else if (Params.Key == EKeys::D) Letter = TEXT('D');
            if (Letter)
            {
                Enemy->SubmitDangerChallengeKey(Letter);
                return true;
            }
        }
    }
    return Super::InputKey(Params);
}

void AParryActionPlayerController::ToggleTargetLock()
{
	if (!IsLocalPlayerController() || !GetWorld() || bWaitingForStart || UGameplayStatics::IsGamePaused(this))
	{
		return;
	}
	if (bTargetLockActive)
	{
		ClearTargetLock();
		return;
	}
	AParryActionCharacter* PlayerCharacter = Cast<AParryActionCharacter>(GetPawn());
	ATrainingEnemy* Enemy = Cast<ATrainingEnemy>(UGameplayStatics::GetActorOfClass(this, ATrainingEnemy::StaticClass()));
	UCharacterMovementComponent* Movement = PlayerCharacter ? PlayerCharacter->GetCharacterMovement() : nullptr;
	const float LockDistance = FMath::Max(1.0f, MaxTargetLockDistance);
	if (!IsValid(PlayerCharacter) || PlayerCharacter->IsDefeated() || !Movement || !IsValid(Enemy) || Enemy->IsDefeated()
		|| FVector::DistSquared2D(PlayerCharacter->GetActorLocation(), Enemy->GetActorLocation()) > FMath::Square(LockDistance))
	{
		return;
	}
	LockedEnemy = Enemy;
	LockCharacter = PlayerCharacter;
	bTargetLockActive = true;
	bPreviousUseControllerYaw = PlayerCharacter->bUseControllerRotationYaw;
	bPreviousOrientToMovement = Movement->bOrientRotationToMovement;
	PlayerCharacter->bUseControllerRotationYaw = true;
	Movement->bOrientRotationToMovement = false;
	if (CombatHUD)
	{
		CombatHUD->SetTargetLockActive(true, Enemy);
	}
}

void AParryActionPlayerController::ClearTargetLock()
{
	if (AParryActionCharacter* PlayerCharacter = LockCharacter.Get())
	{
		PlayerCharacter->bUseControllerRotationYaw = bPreviousUseControllerYaw;
		if (UCharacterMovementComponent* Movement = PlayerCharacter->GetCharacterMovement())
		{
			Movement->bOrientRotationToMovement = bPreviousOrientToMovement;
		}
	}
	LockCharacter.Reset();
	LockedEnemy.Reset();
	bTargetLockActive = false;
	if (CombatHUD)
	{
		CombatHUD->SetTargetLockActive(false);
	}
}

void AParryActionPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (!bTargetLockActive)
	{
		return;
	}
	if (!GetWorld() || UGameplayStatics::IsGamePaused(this))
	{
		return;
	}
	AParryActionCharacter* PlayerCharacter = LockCharacter.Get();
	ATrainingEnemy* Enemy = LockedEnemy.Get();
	const float LockDistance = FMath::Max(1.0f, MaxTargetLockDistance);
	if (!IsValid(PlayerCharacter) || GetPawn() != PlayerCharacter || PlayerCharacter->IsDefeated() || !IsValid(Enemy) || Enemy->IsDefeated()
		|| FVector::DistSquared2D(PlayerCharacter->GetActorLocation(), Enemy->GetActorLocation()) > FMath::Square(LockDistance))
	{
		ClearTargetLock();
		return;
	}
	FVector ToEnemy = Enemy->GetActorLocation() - PlayerCharacter->GetActorLocation();
	ToEnemy.Z = 0.0f;
	if (!ToEnemy.IsNearlyZero())
	{
		const FRotator Current = GetControlRotation();
		const FRotator Desired(Current.Pitch, ToEnemy.Rotation().Yaw, Current.Roll);
		SetControlRotation(FMath::RInterpTo(Current, Desired, DeltaTime, FMath::Max(0.1f, TargetLockTurnSpeed)));
	}
}

void AParryActionPlayerController::RestartAfterResult()
{
	ReloadAfterResult(true);
}

void AParryActionPlayerController::ReturnToStartAfterResult()
{
	ReloadAfterResult(false);
}

void AParryActionPlayerController::ReloadAfterResult(bool bSkipStartScreen)
{
	if (bRestartRequested || !IsLocalPlayerController() || !GetWorld())
	{
		return;
	}
	const AParryActionCharacter* PlayerCharacter = Cast<AParryActionCharacter>(GetPawn());
	const ATrainingEnemy* Enemy = Cast<ATrainingEnemy>(UGameplayStatics::GetActorOfClass(this, ATrainingEnemy::StaticClass()));
	if (!(PlayerCharacter && PlayerCharacter->IsDefeated()) && !(Enemy && Enemy->IsDefeated()))
	{
		return;
	}
	const FString LevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	if (LevelName.IsEmpty())
	{
		return;
	}
	bRestartRequested = true;
#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		for (uint64 Key = 19471; Key <= 19478; ++Key)
		{
			GEngine->RemoveOnScreenDebugMessage(Key);
		}
	}
#endif
	// Absolute travel discards the old URL options when returning to the start screen.
	UGameplayStatics::OpenLevel(this, FName(*LevelName), true, bSkipStartScreen ? TEXT("SkipIntro=1") : TEXT(""));
}

void AParryActionPlayerController::StartCombat()
{
	if (!bWaitingForStart || !IsLocalPlayerController() || !GetWorld())
	{
		return;
	}
	if (SetPause(false))
	{
		bWaitingForStart = false;
		if (CombatHUD)
		{
			CombatHUD->SetStartMenuVisible(false);
		}
	}
}

void AParryActionPlayerController::TogglePause()
{
	if (bWaitingForStart || !IsLocalPlayerController() || !GetWorld())
	{
		return;
	}
	const bool bPause = !UGameplayStatics::IsGamePaused(this);
	if (SetPause(bPause))
	{
		bHelpMenuOpen = false;
		bHelpOpenedFromPause = false;
		if (CombatHUD)
		{
			CombatHUD->SetPauseMenuVisible(bPause);
		}
	}
}

void AParryActionPlayerController::ToggleHelp()
{
	if (bWaitingForStart || !IsLocalPlayerController() || !GetWorld() || !CombatHUD)
	{
		return;
	}
	if (bHelpMenuOpen)
	{
		// Help opened from the pause menu returns to that menu without resuming.
		if (bHelpOpenedFromPause)
		{
			bHelpMenuOpen = false;
			bHelpOpenedFromPause = false;
			CombatHUD->SetPauseMenuVisible(true);
		}
		else if (SetPause(false))
		{
			bHelpMenuOpen = false;
			CombatHUD->SetHelpMenuVisible(false);
		}
	}
	else
	{
		const bool bWasPaused = UGameplayStatics::IsGamePaused(this);
		if (bWasPaused || SetPause(true))
		{
			bHelpOpenedFromPause = bWasPaused;
			bHelpMenuOpen = true;
			CombatHUD->SetHelpMenuVisible(true);
		}
	}
}

void AParryActionPlayerController::QuitFromPause()
{
	if (IsLocalPlayerController() && GetWorld() && UGameplayStatics::IsGamePaused(this))
	{
		UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
	}
}

bool AParryActionPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
