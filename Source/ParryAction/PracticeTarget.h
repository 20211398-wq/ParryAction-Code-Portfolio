#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PracticeTarget.generated.h"

class UStaticMeshComponent;

/** Simple visible target used to test the first attack prototype. */
UCLASS(Blueprintable)
class PARRYACTION_API APracticeTarget : public AActor
{
    GENERATED_BODY()

public:
    APracticeTarget();
    void RegisterPracticeHit();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Target")
    TObjectPtr<UStaticMeshComponent> TargetMesh;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Target")
    int32 HitCount = 0;
};
