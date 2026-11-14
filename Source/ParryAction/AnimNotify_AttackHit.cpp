#include "AnimNotify_AttackHit.h"
#include "ParryActionCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

void UAnimNotify_AttackHit::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);
    if (!MeshComp || !MeshComp->GetWorld() || !MeshComp->GetWorld()->IsGameWorld())
    {
        return;
    }
    AParryActionCharacter* Player = Cast<AParryActionCharacter>(MeshComp->GetOwner());
    if (Player && Player->GetMesh() == MeshComp)
    {
        Player->HandleAttackHitNotify(Animation);
    }
}
