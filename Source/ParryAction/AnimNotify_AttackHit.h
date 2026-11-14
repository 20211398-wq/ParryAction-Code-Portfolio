#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_AttackHit.generated.h"

/** Requests one player attack hit check at this point in the animation. */
UCLASS()
class PARRYACTION_API UAnimNotify_AttackHit : public UAnimNotify
{
    GENERATED_BODY()
public:
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
