// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ParryActionPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UUserWidget;
class UCombatHUDWidget;
class UAudioComponent;
class USoundBase;
class ATrainingEnemy;
class AParryActionCharacter;

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings
 */
UCLASS(abstract)
class AParryActionPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AParryActionPlayerController();
	void StopCombatMusic();
	/** Capture a victory once and keep the best completed time across sessions. */
	void RecordVictory();
	int32 GetVictoryCentiseconds() const { return VictoryCentiseconds; }
	int32 GetBestVictoryCentiseconds() const { return BestVictoryCentiseconds; }
	bool IsNewVictoryRecord() const { return bNewVictoryRecord; }
	bool IsVictoryRecordUnavailable() const { return bVictoryRecordUnavailable; }
	
protected:
	/** Looping background music; use a soft reference to load after engine startup. */
	UPROPERTY(EditDefaultsOnly, Category="Audio")
	TSoftObjectPtr<USoundBase> CombatMusicAsset;

	UPROPERTY(EditDefaultsOnly, Category="Audio", meta=(ClampMin="0.0", ClampMax="1.0"))
	float CombatMusicVolume = 0.12f;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> CombatMusicComponent;

	/** Widget blueprint that displays health, combat cues and results. */
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UCombatHUDWidget> CombatHUDClass;

	UPROPERTY(Transient)
	TObjectPtr<UCombatHUDWidget> CombatHUD;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PlayerTick(float DeltaTime) override;
    virtual bool InputKey(const FInputKeyEventArgs& Params) override;

	/** Single-enemy lock range and camera rotation interpolation speed. */
	UPROPERTY(EditDefaultsOnly, Category="Target Lock", meta=(ClampMin="1.0"))
	float MaxTargetLockDistance = 2000.0f;

	UPROPERTY(EditDefaultsOnly, Category="Target Lock", meta=(ClampMin="0.1"))
	float TargetLockTurnSpeed = 7.0f;

	TWeakObjectPtr<ATrainingEnemy> LockedEnemy;
	TWeakObjectPtr<AParryActionCharacter> LockCharacter;
	bool bTargetLockActive = false;
	bool bPreviousUseControllerYaw = false;
	bool bPreviousOrientToMovement = true;
	void ToggleTargetLock();
	void ClearTargetLock();

	/** Restart input stays on the controller when the character is defeated. */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* RestartAction = nullptr;

	bool bVictoryRecorded = false;
	bool bNewVictoryRecord = false;
	bool bVictoryRecordUnavailable = false;
	int32 VictoryCentiseconds = -1;
	int32 BestVictoryCentiseconds = -1;
	bool bRestartRequested = false;
	bool bWaitingForStart = false;
	bool bHelpMenuOpen = false;
	bool bHelpOpenedFromPause = false;
	void RestartAfterResult();
	void ReturnToStartAfterResult();
	void ReloadAfterResult(bool bSkipStartScreen);
	void StartCombat();
	void TogglePause();
	void ToggleHelp();
	void QuitFromPause();


	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

};
