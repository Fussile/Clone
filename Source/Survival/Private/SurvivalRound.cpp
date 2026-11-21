#include "SurvivalRound.h"

#include "CollisionQueryParams.h"
#include "Components/SphereComponent.h"
#include "Engine/HitResult.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "SurvivalBuildChunk.h"
#include "SurvivalCharacter.h"
#include "SurvivalZombie.h"

ASurvivalRound::ASurvivalRound()
{
    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    SetRootComponent(Collision);
    Collision->InitSphereRadius(1.0f);
    Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Collision->SetCollisionObjectType(ECC_WorldDynamic);
    Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
    Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
    Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    Collision->SetGenerateOverlapEvents(false);
    Collision->SetCanEverAffectNavigation(false);

    Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Flight"));
    Movement->SetUpdatedComponent(Collision);
    Movement->InitialSpeed = 9000.0f;
    Movement->Velocity = FVector::ForwardVector;
    Movement->bInitialVelocityInLocalSpace = true;
    Movement->bSweepCollision = true;
    Movement->ProjectileGravityScale = 0.5f;
    Movement->OnProjectileStop.AddDynamic(this, &ASurvivalRound::HandleImpact);
    InitialLifeSpan = 0.65f;
}

void ASurvivalRound::SetRifleRound()
{
    bRifleRound = true;
    Movement->Velocity *= 1.8;
    SetLifeSpan(0.8f);
}

void ASurvivalRound::BeginPlay()
{
    Super::BeginPlay();
    if (APawn* Shooter = GetInstigator())
    {
        Collision->IgnoreActorWhenMoving(Shooter, true);
    }
}

void ASurvivalRound::HandleImpact(const FHitResult& Hit)
{
    ASurvivalCharacter* Shooter = Cast<ASurvivalCharacter>(GetInstigator());
    bool bDamaged = false;
    bool bHitZombie = false;
    if (ASurvivalBuildChunk* Chunk = Cast<ASurvivalBuildChunk>(Hit.GetActor()))
    {
        FHitResult InstanceHit = Hit;
        const FSurvivalFinishedPiece* Piece = Chunk->FindFinished(InstanceHit);
        if (!Piece && Hit.GetComponent())
        {
            // Resolve an instance if the movement sweep reports only the instanced component.
            const FVector Offset = Hit.ImpactNormal * 3.0;
            if (Hit.GetComponent()->LineTraceComponent(InstanceHit, Hit.ImpactPoint + Offset,
                Hit.ImpactPoint - Offset, FCollisionQueryParams(SCENE_QUERY_STAT(RoundInstance))))
            {
                Piece = Chunk->FindFinished(InstanceHit);
            }
        }
        if (Piece)
        {
            bDamaged = Chunk->DamageFinished(InstanceHit, Piece->Placement.GetPiece().bStone ? (bRifleRound ? 9 : 4) : (bRifleRound ? 26 : 12));
        }
    }
    else if (ASurvivalZombie* Zombie = Cast<ASurvivalZombie>(Hit.GetActor()))
    {
        float Damage = bRifleRound ? 70.0f : 32.0f;
        if (Shooter)
        {
            const FSurvivalProgression& Progress = Shooter->GetProgression();
            Damage *= Progress.Effect(bRifleRound ? ETraining::Rifles : ETraining::Pistols);
            Damage *= Progress.Effect(bRifleRound ? ETraining::RifleTechnique : ETraining::PistolTechnique);
        }
        Zombie->HitByShot(Shooter, FMath::RoundToInt(Damage));
        bDamaged = true;
        bHitZombie = true;
    }

    if (bDamaged && Shooter)
    {
        Shooter->NotifyRoundHit(bHitZombie, bRifleRound);
    }
    Destroy();
}
