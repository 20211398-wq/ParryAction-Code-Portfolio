#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UBorder;
class ATrainingEnemy;

/** Runtime combat HUD. Data comes from the active player and single training enemy. */
UCLASS(Blueprintable)
class PARRYACTION_API UCombatHUDWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetPauseMenuVisible(bool bVisible);
    void SetHelpMenuVisible(bool bVisible);
    void SetStartMenuVisible(bool bVisible);
    void SetStartBestRecord(int32 BestCentiseconds, bool bUnavailable);
    void SetTargetLockActive(bool bActive, ATrainingEnemy* Target = nullptr);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    UPROPERTY(Transient) TObjectPtr<UTextBlock> PlayerHealthText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DodgeCooldownText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> EnemyHealthText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> CombatTimeText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> CombatStatusText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> TargetLockText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> TargetMarker;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> CounterWindowText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> EnemyWindupText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DangerChallengeText;
    UPROPERTY(Transient) TObjectPtr<UProgressBar> DangerChallengeBar;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ResultText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> PauseText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> StartText;
    UPROPERTY(Transient) TObjectPtr<UBorder> HelpOverlay;
    UPROPERTY(Transient) TObjectPtr<UBorder> StartOverlay;
    UPROPERTY(Transient) TObjectPtr<UProgressBar> PlayerHealthBar;
    UPROPERTY(Transient) TObjectPtr<UProgressBar> DodgeCooldownBar;
    UPROPERTY(Transient) TObjectPtr<UProgressBar> EnemyHealthBar;
    UPROPERTY(Transient) TObjectPtr<UProgressBar> ParryWindowBar;
    UPROPERTY(Transient) TObjectPtr<UProgressBar> CounterWindowBar;
    UPROPERTY(Transient) TObjectPtr<UProgressBar> EnemyWindupBar;
    TWeakObjectPtr<ATrainingEnemy> CachedEnemy;
    TWeakObjectPtr<ATrainingEnemy> MarkerTarget;
    float LastDisplayedPlayerHealth = -1.0f;
    float PlayerDamageFlashEndTime = -1.0f;
    float ResultElapsedSeconds = -1.0f;
};
