#include "CombatHUDWidget.h"
#include "ParryActionCharacter.h"
#include "ParryActionPlayerController.h"
#include "TrainingEnemy.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Styling/CoreStyle.h"

namespace
{
    FString FormatCombatTime(int32 Centiseconds)
    {
        const int32 TotalSeconds = Centiseconds / 100;
        return FString::Printf(TEXT("%02d:%02d.%02d"), TotalSeconds / 60, TotalSeconds % 60, Centiseconds % 100);
    }
}

void UCombatHUDWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (!WidgetTree)
    {
        return;
    }
    SetVisibility(ESlateVisibility::HitTestInvisible);
    UCanvasPanel* Canvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
    if (!Canvas)
    {
        UWidget* PreviousRoot = WidgetTree->RootWidget;
        Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CombatCanvas"));
        WidgetTree->RootWidget = Canvas;
        if (PreviousRoot)
        {
            UCanvasPanelSlot* PreviousSlot = Canvas->AddChildToCanvas(PreviousRoot);
            PreviousSlot->SetAnchors(FAnchors(0, 0, 1, 1));
            PreviousSlot->SetOffsets(FMargin(0));
        }
    }

    auto MakeText = [this](FName Name, int32 FontSize, const FLinearColor& Color)
    {
        UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
        Text->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FontSize));
        Text->SetColorAndOpacity(FSlateColor(Color));
        Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
        Text->SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f));
        return Text;
    };

    UVerticalBox* PlayerBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PlayerHealthBox"));
    UCanvasPanelSlot* PlayerSlot = Canvas->AddChildToCanvas(PlayerBox);
    PlayerSlot->SetAnchors(FAnchors(0, 0, 0, 0));
    PlayerSlot->SetPosition(FVector2D(28, 28));
    PlayerSlot->SetSize(FVector2D(320, 70));
    PlayerHealthText = MakeText(TEXT("PlayerHealthText"), 18, FLinearColor::White);
    PlayerHealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("PlayerHealthBar"));
    PlayerHealthBar->SetFillColorAndOpacity(FLinearColor(0.1f, 0.75f, 0.9f, 1));
    PlayerBox->AddChildToVerticalBox(PlayerHealthText);
    PlayerBox->AddChildToVerticalBox(PlayerHealthBar);

    // This display reads the same cooldown and readiness used by Dodge().
    DodgeCooldownText = MakeText(TEXT("DodgeCooldownText"), 15, FLinearColor::White);
    UCanvasPanelSlot* DodgeTextSlot = Canvas->AddChildToCanvas(DodgeCooldownText);
    DodgeTextSlot->SetAnchors(FAnchors(0, 0, 0, 0));
    DodgeTextSlot->SetPosition(FVector2D(28, 105));
    DodgeTextSlot->SetAutoSize(true);
    DodgeCooldownBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("DodgeCooldownBar"));
    UCanvasPanelSlot* DodgeBarSlot = Canvas->AddChildToCanvas(DodgeCooldownBar);
    DodgeBarSlot->SetAnchors(FAnchors(0, 0, 0, 0));
    DodgeBarSlot->SetPosition(FVector2D(28, 128));
    DodgeBarSlot->SetSize(FVector2D(200, 12));

    UVerticalBox* EnemyBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("EnemyHealthBox"));
    UCanvasPanelSlot* EnemySlot = Canvas->AddChildToCanvas(EnemyBox);
    EnemySlot->SetAnchors(FAnchors(1, 0, 1, 0));
    EnemySlot->SetAlignment(FVector2D(1, 0));
    EnemySlot->SetPosition(FVector2D(-28, 28));
    EnemySlot->SetSize(FVector2D(320, 70));
    EnemyHealthText = MakeText(TEXT("EnemyHealthText"), 18, FLinearColor::White);
    EnemyHealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("EnemyHealthBar"));
    EnemyHealthBar->SetFillColorAndOpacity(FLinearColor(0.95f, 0.25f, 0.16f, 1));
    EnemyBox->AddChildToVerticalBox(EnemyHealthText);
    EnemyBox->AddChildToVerticalBox(EnemyHealthBar);

    // Show the same game-time source used by the final result and best record.
    CombatTimeText = MakeText(TEXT("CombatTimeText"), 20, FLinearColor::White);
    CombatTimeText->SetText(FText::FromString(TEXT("TIME  00:00.00")));
    CombatTimeText->SetJustification(ETextJustify::Center);
    UCanvasPanelSlot* TimeSlot = Canvas->AddChildToCanvas(CombatTimeText);
    TimeSlot->SetAnchors(FAnchors(0.5f, 0, 0.5f, 0));
    TimeSlot->SetAlignment(FVector2D(0.5f, 0));
    TimeSlot->SetPosition(FVector2D(0, 28));
    TimeSlot->SetAutoSize(true);

    TargetMarker = MakeText(TEXT("TargetMarker"), 25, FLinearColor(1.0f, 0.9f, 0.25f, 1.0f));
    TargetMarker->SetText(FText::FromString(TEXT("[+]")));
    TargetMarker->SetVisibility(ESlateVisibility::Collapsed);
    UCanvasPanelSlot* TargetMarkerSlot = Canvas->AddChildToCanvas(TargetMarker);
    TargetMarkerSlot->SetAnchors(FAnchors(0, 0, 0, 0));
    TargetMarkerSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    TargetMarkerSlot->SetAutoSize(true);

    TargetLockText = MakeText(TEXT("TargetLockText"), 16, FLinearColor(0.35f, 0.85f, 1.0f, 1));
    TargetLockText->SetText(FText::FromString(TEXT("LOCK: OFF")));
    UCanvasPanelSlot* LockSlot = Canvas->AddChildToCanvas(TargetLockText);
    LockSlot->SetAnchors(FAnchors(1, 0, 1, 0));
    LockSlot->SetAlignment(FVector2D(1, 0));
    LockSlot->SetPosition(FVector2D(-28, 105));
    LockSlot->SetAutoSize(true);

    // This gauge reads the enemy's existing stun timer; it never controls combat.
    CounterWindowText = MakeText(TEXT("CounterWindowText"), 15, FLinearColor(0.35f, 0.85f, 1.0f, 1.0f));
    CounterWindowText->SetText(FText::FromString(TEXT("COUNTER WINDOW")));
    CounterWindowText->SetVisibility(ESlateVisibility::Collapsed);
    UCanvasPanelSlot* CounterTextSlot = Canvas->AddChildToCanvas(CounterWindowText);
    CounterTextSlot->SetAnchors(FAnchors(1, 0, 1, 0));
    CounterTextSlot->SetAlignment(FVector2D(1, 0));
    CounterTextSlot->SetPosition(FVector2D(-28, 132));
    CounterTextSlot->SetAutoSize(true);

    CounterWindowBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("CounterWindowBar"));
    CounterWindowBar->SetFillColorAndOpacity(FLinearColor(0.08f, 0.75f, 1.0f, 1.0f));
    CounterWindowBar->SetVisibility(ESlateVisibility::Collapsed);
    UCanvasPanelSlot* CounterBarSlot = Canvas->AddChildToCanvas(CounterWindowBar);
    CounterBarSlot->SetAnchors(FAnchors(1, 0, 1, 0));
    CounterBarSlot->SetAlignment(FVector2D(1, 0));
    CounterBarSlot->SetPosition(FVector2D(-28, 155));
    CounterBarSlot->SetSize(FVector2D(200, 12));

    CombatStatusText = MakeText(TEXT("CombatStatusText"), 23, FLinearColor(1, 0.85f, 0.25f, 1));
    CombatStatusText->SetJustification(ETextJustify::Center);
    UCanvasPanelSlot* StatusSlot = Canvas->AddChildToCanvas(CombatStatusText);
    StatusSlot->SetAnchors(FAnchors(0.5f, 0, 0.5f, 0));
    StatusSlot->SetAlignment(FVector2D(0.5f, 0));
    StatusSlot->SetPosition(FVector2D(0, 100));
    StatusSlot->SetAutoSize(true);

    // A short-lived gauge makes the actual defensive window easier to read.
    ParryWindowBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ParryWindowBar"));
    ParryWindowBar->SetFillColorAndOpacity(FLinearColor(0.08f, 0.75f, 1.0f, 1.0f));
    ParryWindowBar->SetVisibility(ESlateVisibility::Collapsed);
    UCanvasPanelSlot* ParrySlot = Canvas->AddChildToCanvas(ParryWindowBar);
    ParrySlot->SetAnchors(FAnchors(0.5f, 0, 0.5f, 0));
    ParrySlot->SetAlignment(FVector2D(0.5f, 0));
    ParrySlot->SetPosition(FVector2D(0, 145));
    ParrySlot->SetSize(FVector2D(240, 16));

    // The enemy's actual windup timer drives this presentation-only gauge.
    EnemyWindupText = MakeText(TEXT("EnemyWindupText"), 14, FLinearColor(1.0f, 0.65f, 0.02f, 1.0f));
    EnemyWindupText->SetText(FText::FromString(TEXT("ENEMY ATTACK")));
    EnemyWindupText->SetJustification(ETextJustify::Center);
    EnemyWindupText->SetVisibility(ESlateVisibility::Collapsed);
    UCanvasPanelSlot* WindupTextSlot = Canvas->AddChildToCanvas(EnemyWindupText);
    WindupTextSlot->SetAnchors(FAnchors(0.5f, 0, 0.5f, 0));
    WindupTextSlot->SetAlignment(FVector2D(0.5f, 0));
    WindupTextSlot->SetPosition(FVector2D(0, 174));
    WindupTextSlot->SetAutoSize(true);

    EnemyWindupBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("EnemyWindupBar"));
    EnemyWindupBar->SetVisibility(ESlateVisibility::Collapsed);
    UCanvasPanelSlot* WindupBarSlot = Canvas->AddChildToCanvas(EnemyWindupBar);
    WindupBarSlot->SetAnchors(FAnchors(0.5f, 0, 0.5f, 0));
    WindupBarSlot->SetAlignment(FVector2D(0.5f, 0));
    WindupBarSlot->SetPosition(FVector2D(0, 196));
    WindupBarSlot->SetSize(FVector2D(200, 12));

    DangerChallengeText = MakeText(TEXT("DangerChallengeText"), 28, FLinearColor::White);
    DangerChallengeText->SetJustification(ETextJustify::Center);
    DangerChallengeText->SetVisibility(ESlateVisibility::Collapsed);
    UCanvasPanelSlot* ChallengeSlot = Canvas->AddChildToCanvas(DangerChallengeText);
    ChallengeSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
    ChallengeSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    ChallengeSlot->SetPosition(FVector2D(0, -20));
    ChallengeSlot->SetAutoSize(true);
    DangerChallengeBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("DangerChallengeBar"));
    DangerChallengeBar->SetFillColorAndOpacity(FLinearColor(1.0f, 0.45f, 0.08f, 1.0f));
    DangerChallengeBar->SetVisibility(ESlateVisibility::Collapsed);
    UCanvasPanelSlot* ChallengeBarSlot = Canvas->AddChildToCanvas(DangerChallengeBar);
    ChallengeBarSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
    ChallengeBarSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    ChallengeBarSlot->SetPosition(FVector2D(0, 40));
    ChallengeBarSlot->SetSize(FVector2D(260, 16));

    ResultText = MakeText(TEXT("ResultText"), 36, FLinearColor::White);
    ResultText->SetJustification(ETextJustify::Center);
    UCanvasPanelSlot* ResultSlot = Canvas->AddChildToCanvas(ResultText);
    ResultSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
    ResultSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    ResultSlot->SetPosition(FVector2D::ZeroVector);
    ResultSlot->SetAutoSize(true);

    PauseText = MakeText(TEXT("PauseText"), 36, FLinearColor::White);
    PauseText->SetText(FText::FromString(TEXT("PAUSED\nP: RESUME    H: HELP    Q: QUIT")));
    PauseText->SetJustification(ETextJustify::Center);
    PauseText->SetVisibility(ESlateVisibility::Collapsed);
    UCanvasPanelSlot* PauseSlot = Canvas->AddChildToCanvas(PauseText);
    PauseSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
    PauseSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    PauseSlot->SetPosition(FVector2D::ZeroVector);
    PauseSlot->SetAutoSize(true);

    UTextBlock* Controls = MakeText(TEXT("ControlsText"), 14, FLinearColor(0.85f, 0.85f, 0.85f, 1));
    Controls->SetText(FText::FromString(TEXT("LMB ATTACK    RMB PARRY    SHIFT DODGE    TAB LOCK    P PAUSE    H HELP")));
    UCanvasPanelSlot* ControlsSlot = Canvas->AddChildToCanvas(Controls);
    ControlsSlot->SetAnchors(FAnchors(0.5f, 1, 0.5f, 1));
    ControlsSlot->SetAlignment(FVector2D(0.5f, 1));
    ControlsSlot->SetPosition(FVector2D(0, -24));
    ControlsSlot->SetAutoSize(true);

    HelpOverlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("HelpOverlay"));
    HelpOverlay->SetBrush(*FCoreStyle::Get().GetBrush("WhiteBrush"));
    HelpOverlay->SetBrushColor(FLinearColor(0.01f, 0.02f, 0.04f, 0.92f));
    HelpOverlay->SetHorizontalAlignment(HAlign_Center);
    HelpOverlay->SetVerticalAlignment(VAlign_Center);
    HelpOverlay->SetVisibility(ESlateVisibility::Collapsed);
    UTextBlock* HelpText = MakeText(TEXT("HelpText"), 21, FLinearColor::White);
    HelpText->SetJustification(ETextJustify::Center);
    HelpText->SetText(FText::FromString(TEXT(
        "HOW TO PLAY\n\n"
        "WASD MOVE    MOUSE LOOK    SPACE JUMP\n"
        "LMB ATTACK    RMB PARRY    SHIFT DODGE\n"
        "TAB LOCK ON / OFF\n\n"
        "ORANGE ATTACK: PARRY JUST BEFORE IMPACT\n"
        "RED ATTACK: DODGE OR PARRY FIRST STRIKE\n"
        "THEN PRESS THE SHOWN WASD SEQUENCE\n"
        "WRONG INPUT OR TIMEOUT: HEAVY DAMAGE\n"
        "PARRY STUNS THE ENEMY: ATTACK TO COUNTER\n"
        "DODGING BRIEFLY PREVENTS DAMAGE\n\n"
        "H: CLOSE HELP    P: RESUME    Q: QUIT")));
    HelpOverlay->SetContent(HelpText);
    UCanvasPanelSlot* HelpSlot = Canvas->AddChildToCanvas(HelpOverlay);
    HelpSlot->SetAnchors(FAnchors(0, 0, 1, 1));
    HelpSlot->SetOffsets(FMargin(0));

    StartOverlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StartOverlay"));
    StartOverlay->SetBrush(*FCoreStyle::Get().GetBrush("WhiteBrush"));
    StartOverlay->SetBrushColor(FLinearColor(0.01f, 0.02f, 0.04f, 0.94f));
    StartOverlay->SetHorizontalAlignment(HAlign_Center);
    StartOverlay->SetVerticalAlignment(VAlign_Center);
    StartOverlay->SetVisibility(ESlateVisibility::Collapsed);
    StartText = MakeText(TEXT("StartText"), 27, FLinearColor::White);
    StartText->SetJustification(ETextJustify::Center);
    SetStartBestRecord(-1, false);
    StartOverlay->SetContent(StartText);
    UCanvasPanelSlot* StartSlot = Canvas->AddChildToCanvas(StartOverlay);
    StartSlot->SetAnchors(FAnchors(0, 0, 1, 1));
    StartSlot->SetOffsets(FMargin(0));
}

void UCombatHUDWidget::SetStartBestRecord(int32 BestCentiseconds, bool bUnavailable)
{
    if (!StartText)
    {
        return;
    }
    const FString Best = bUnavailable ? TEXT("UNAVAILABLE")
        : (BestCentiseconds >= 0 ? FormatCombatTime(BestCentiseconds) : TEXT("--:--.--"));
    StartText->SetText(FText::FromString(FString::Printf(TEXT(
        "PARRY ACTION\n\n"
        "PARRY. DODGE. COUNTER.\n\n"
        "BEST: %s\n\n"
        "ENTER: START    Q: QUIT\n\n"
        "PRESS H DURING BATTLE FOR HELP"), *Best)));
}

void UCombatHUDWidget::SetPauseMenuVisible(bool bVisible)
{
    if (HelpOverlay)
    {
        HelpOverlay->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (PauseText)
    {
        PauseText->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    if (ResultText)
    {
        ResultText->SetVisibility(bVisible ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    }
}

void UCombatHUDWidget::SetHelpMenuVisible(bool bVisible)
{
    if (HelpOverlay)
    {
        HelpOverlay->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    if (PauseText)
    {
        PauseText->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (ResultText)
    {
        ResultText->SetVisibility(bVisible ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    }
}

void UCombatHUDWidget::SetStartMenuVisible(bool bVisible)
{
    if (StartOverlay)
    {
        StartOverlay->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    if (ResultText)
    {
        ResultText->SetVisibility(bVisible ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    }
}

void UCombatHUDWidget::SetTargetLockActive(bool bActive, ATrainingEnemy* Target)
{
    MarkerTarget = bActive ? Target : nullptr;
    if (TargetMarker && !MarkerTarget.IsValid())
    {
        TargetMarker->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (TargetLockText)
    {
        TargetLockText->SetText(FText::FromString(bActive ? TEXT("LOCK: ON") : TEXT("LOCK: OFF")));
    }
}

void UCombatHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!PlayerHealthText || !EnemyHealthText || !PlayerHealthBar || !EnemyHealthBar || !CombatStatusText || !ResultText)
    {
        return;
    }
    const AParryActionCharacter* Player = Cast<AParryActionCharacter>(GetOwningPlayerPawn());
    if (!CachedEnemy.IsValid() && GetWorld())
    {
        CachedEnemy = Cast<ATrainingEnemy>(UGameplayStatics::GetActorOfClass(this, ATrainingEnemy::StaticClass()));
    }
    const ATrainingEnemy* Enemy = CachedEnemy.Get();
    if (TargetMarker)
    {
        ATrainingEnemy* Target = MarkerTarget.Get();
        FVector2D ScreenPosition = FVector2D::ZeroVector;
        UCanvasPanelSlot* MarkerSlot = Cast<UCanvasPanelSlot>(TargetMarker->Slot);
        const bool bShowMarker = IsValid(Player) && !Player->IsDefeated() && IsValid(Target) && !Target->IsDefeated()
            && MarkerSlot && GetOwningPlayer()
            && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(),
                Target->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f), ScreenPosition, false);
        TargetMarker->SetVisibility(bShowMarker ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (bShowMarker)
        {
            MarkerSlot->SetPosition(ScreenPosition);
        }
    }
    if (!Player || !Enemy)
    {
        if (CombatTimeText) CombatTimeText->SetVisibility(ESlateVisibility::Collapsed);
        if (DodgeCooldownText) DodgeCooldownText->SetVisibility(ESlateVisibility::Collapsed);
        if (DodgeCooldownBar) DodgeCooldownBar->SetVisibility(ESlateVisibility::Collapsed);
        if (CounterWindowText) CounterWindowText->SetVisibility(ESlateVisibility::Collapsed);
        if (CounterWindowBar) CounterWindowBar->SetVisibility(ESlateVisibility::Collapsed);
        if (EnemyWindupText) EnemyWindupText->SetVisibility(ESlateVisibility::Collapsed);
        if (EnemyWindupBar) EnemyWindupBar->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }
    const bool bChallenge = Enemy->IsDangerChallengeFor(Player) && !Player->IsDefeated();
    if (DangerChallengeText)
    {
        DangerChallengeText->SetVisibility(bChallenge ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (bChallenge) DangerChallengeText->SetText(FText::FromString(FString::Printf(TEXT("PARRY CHALLENGE\n%s"), *Enemy->GetDangerChallengePrompt())));
    }
    if (DangerChallengeBar)
    {
        DangerChallengeBar->SetVisibility(bChallenge ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (bChallenge) DangerChallengeBar->SetPercent(Enemy->GetDangerChallengeFraction());
    }
    const float PlayerHealth = Player->GetCurrentHealth();
    const UWorld* World = GetWorld();
    if (World && LastDisplayedPlayerHealth >= 0.0f && PlayerHealth < LastDisplayedPlayerHealth)
    {
        PlayerDamageFlashEndTime = World->GetTimeSeconds() + 0.4f;
    }
    LastDisplayedPlayerHealth = PlayerHealth;
    const bool bShowDamageFlash = World && World->GetTimeSeconds() < PlayerDamageFlashEndTime;
    PlayerHealthText->SetColorAndOpacity(FSlateColor(bShowDamageFlash ? FLinearColor(1.0f, 0.12f, 0.12f, 1.0f) : FLinearColor::White));
    PlayerHealthText->SetText(FText::FromString(FString::Printf(TEXT("PLAYER  %.0f / %.0f"), PlayerHealth, Player->GetMaxHealth())));
    PlayerHealthBar->SetPercent(FMath::Clamp(Player->GetCurrentHealth() / FMath::Max(1.0f, Player->GetMaxHealth()), 0.0f, 1.0f));
    if (ParryWindowBar)
    {
        ParryWindowBar->SetVisibility(Player->IsParryActive() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        ParryWindowBar->SetPercent(Player->GetParryWindowFraction());
    }
    EnemyHealthText->SetText(FText::FromString(FString::Printf(TEXT("ENEMY  %.0f / %.0f"), Enemy->GetCurrentHealth(), Enemy->GetMaxHealth())));
    EnemyHealthBar->SetPercent(FMath::Clamp(Enemy->GetCurrentHealth() / FMath::Max(1.0f, Enemy->GetMaxHealth()), 0.0f, 1.0f));

    const bool bPlayerDefeated = Player->IsDefeated();
    const bool bEnemyDefeated = Enemy->IsDefeated();
    const bool bShowDodge = !bPlayerDefeated && !bEnemyDefeated;
    const ESlateVisibility DodgeVisibility = bShowDodge ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
    const bool bDodgeReady = Player->IsDodgeReady();
    const float DodgeFraction = Player->GetDodgeCooldownFraction();
    const FLinearColor DodgeColor = bDodgeReady ? FLinearColor(0.35f, 0.9f, 0.55f, 1.0f) : FLinearColor(0.75f, 0.75f, 0.75f, 1.0f);
    if (DodgeCooldownText)
    {
        const TCHAR* Label = bDodgeReady ? TEXT("DODGE: READY")
            : (DodgeFraction < 1.0f ? TEXT("DODGE: COOLDOWN") : TEXT("DODGE: UNAVAILABLE"));
        DodgeCooldownText->SetText(FText::FromString(Label));
        DodgeCooldownText->SetColorAndOpacity(FSlateColor(DodgeColor));
        DodgeCooldownText->SetVisibility(DodgeVisibility);
    }
    if (DodgeCooldownBar)
    {
        DodgeCooldownBar->SetVisibility(DodgeVisibility);
        DodgeCooldownBar->SetFillColorAndOpacity(DodgeColor);
        DodgeCooldownBar->SetPercent(DodgeFraction);
    }
    if (CombatTimeText)
    {
        const bool bBattleActive = !bPlayerDefeated && !bEnemyDefeated;
        CombatTimeText->SetVisibility(bBattleActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (bBattleActive)
        {
            const int32 Centiseconds = FMath::Max(0, FMath::FloorToInt(Player->GetCombatElapsedSeconds() * 100.0f));
            CombatTimeText->SetText(FText::FromString(FString::Printf(TEXT("TIME  %s"), *FormatCombatTime(Centiseconds))));
        }
    }
    const bool bShowEnemyWindup = !bPlayerDefeated && !bEnemyDefeated && Enemy->IsWindingUp();
    const ESlateVisibility WindupVisibility = bShowEnemyWindup ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
    const bool bDangerWindup = bShowEnemyWindup && Enemy->IsUnblockableAttack();
    const FLinearColor WindupColor = bDangerWindup
        ? FLinearColor(1.0f, 0.1f, 0.05f, 1.0f) : FLinearColor(1.0f, 0.65f, 0.02f, 1.0f);
    if (EnemyWindupText)
    {
        EnemyWindupText->SetVisibility(WindupVisibility);
        const int32 DangerStrike = Enemy->GetDangerStrikeNumber();
        const TCHAR* WindupLabel = !bDangerWindup ? TEXT("ENEMY ATTACK")
            : (DangerStrike == 2 ? TEXT("DANGER ATTACK  2 / 2") : TEXT("DANGER ATTACK  1 / 2"));
        EnemyWindupText->SetText(FText::FromString(WindupLabel));
        EnemyWindupText->SetColorAndOpacity(FSlateColor(WindupColor));
    }
    if (EnemyWindupBar)
    {
        EnemyWindupBar->SetVisibility(WindupVisibility);
        EnemyWindupBar->SetFillColorAndOpacity(WindupColor);
        EnemyWindupBar->SetPercent(Enemy->GetAttackWindupFraction());
    }
    const bool bShowCounterWindow = !bPlayerDefeated && !bEnemyDefeated && Enemy->IsStunned();
    const ESlateVisibility CounterVisibility = bShowCounterWindow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
    if (CounterWindowText) CounterWindowText->SetVisibility(CounterVisibility);
    if (CounterWindowBar)
    {
        CounterWindowBar->SetVisibility(CounterVisibility);
        CounterWindowBar->SetPercent(Enemy->GetParryStunFraction());
    }
    if (bPlayerDefeated || bEnemyDefeated)
    {
        // Defeat time is captured on the player; victory time is captured at the final hit.
        if (ResultElapsedSeconds < 0.0f)
        {
            ResultElapsedSeconds = Player->GetCombatElapsedSeconds();
        }
        const AParryActionPlayerController* Controller = Cast<AParryActionPlayerController>(GetOwningPlayer());
        const int32 VictoryTime = Controller ? Controller->GetVictoryCentiseconds() : -1;
        const int32 ResultCentiseconds = !bPlayerDefeated && VictoryTime >= 0
            ? VictoryTime : FMath::Max(0, FMath::FloorToInt(ResultElapsedSeconds * 100.0f));
        FString RecordLine;
        if (!bPlayerDefeated && Controller)
        {
            const int32 Best = Controller->GetBestVictoryCentiseconds();
            if (Best >= 0)
            {
                RecordLine = FString::Printf(TEXT("\nBEST: %s%s"), *FormatCombatTime(Best),
                    Controller->IsNewVictoryRecord() ? TEXT("  NEW!") : TEXT(""));
            }
            if (Controller->IsVictoryRecordUnavailable())
            {
                RecordLine += TEXT("\nRECORD SAVE UNAVAILABLE");
            }
        }
        const TCHAR* Outcome = bPlayerDefeated ? TEXT("DEFEAT") : TEXT("VICTORY");
        ResultText->SetText(FText::FromString(FString::Printf(
            TEXT("%s\nPARRIES: %d\nDAMAGE TAKEN: %.0f\nTIME: %s%s\nR: RETRY    T: TITLE"),
            Outcome, Player->GetSuccessfulParryCount(), Player->GetTotalDamageTaken(),
            *FormatCombatTime(ResultCentiseconds), *RecordLine)));
    }
    else
    {
        ResultText->SetText(FText::GetEmpty());
    }
    const TCHAR* Status = TEXT("");
    const bool bDanger = !bPlayerDefeated && !bEnemyDefeated && Enemy->IsWindingUp() && Enemy->IsUnblockableAttack();
    CombatStatusText->SetColorAndOpacity(FSlateColor(bDanger ? FLinearColor(1.0f, 0.15f, 0.1f, 1.0f) : FLinearColor(1.0f, 0.85f, 0.25f, 1.0f)));
    if (!bPlayerDefeated && !bEnemyDefeated)
    {
        if (Enemy->IsStunned())
        {
            Status = Player->HasRecentAttackResult() && Player->DidLastAttackCounter() ? TEXT("COUNTER HIT!") : TEXT("PARRY SUCCESS! COUNTER!");
        }
        else if (bChallenge) Status = TEXT("ENTER THE SEQUENCE!");
        else if (bDanger) Status = TEXT("DANGER! DODGE OR PARRY FIRST!");
        else if (Player->IsDodgeInvulnerable()) Status = TEXT("DODGE WINDOW");
        else if (Enemy->IsWindingUp()) Status = Player->IsParryActive() ? TEXT("INCOMING - PARRY WINDOW") : TEXT("INCOMING ATTACK!");
        else if (Enemy->GetRecentAttackOutcome() == 4) Status = TEXT("DODGE SUCCESS!");
        else if (Enemy->GetRecentAttackOutcome() == 1) Status = TEXT("ATTACK EVADED");
        else if (Enemy->GetRecentAttackOutcome() == 2) Status = TEXT("PLAYER HIT");
        else if (Player->IsParryActive()) Status = TEXT("PARRY WINDOW");
        else if (Player->HasRecentAttackResult()) Status = Player->DidLastAttackHit() ? TEXT("ATTACK HIT") : TEXT("ATTACK MISS");
    }
    CombatStatusText->SetText(FText::FromString(Status));
}
