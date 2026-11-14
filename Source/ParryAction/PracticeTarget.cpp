#include "PracticeTarget.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

APracticeTarget::APracticeTarget()
{
    PrimaryActorTick.bCanEverTick = false;
    TargetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TargetMesh"));
    RootComponent = TargetMesh;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cube.Succeeded())
    {
        TargetMesh->SetStaticMesh(Cube.Object);
    }
    TargetMesh->SetRelativeScale3D(FVector(0.7f, 0.7f, 1.6f));
    TargetMesh->SetCollisionProfileName(TEXT("BlockAll"));
}

void APracticeTarget::RegisterPracticeHit()
{
    ++HitCount;
}
