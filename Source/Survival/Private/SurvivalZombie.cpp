#include "SurvivalZombie.h"

#include "Animation/AnimSequence.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "SurvivalBuildChunk.h"
#include "SurvivalCharacter.h"

ASurvivalZombie::ASurvivalZombie()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetCharacterMovement()->bRunPhysicsWithNoController = true;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->MaxWalkSpeed = 235.0f;
    bUseControllerRotationYaw = false;
}

void ASurvivalZombie::BeginPlay()
{
    Super::BeginPlay();
    USkeletalMesh* ZombieAsset = LoadObject<USkeletalMesh>(nullptr,
        TEXT("/Game/Art/Crossroads/Survivor/characterMedium.characterMedium"));
    if (!ZombieAsset)
    {
        return;
    }
    USkeletalMeshComponent* Body = GetMesh();
    Body->SetSkeletalMesh(ZombieAsset);
    const FBoxSphereBounds Bounds = ZombieAsset->GetBounds();
    const double Scale = 175.0 / FMath::Max(1.0, 2.0 * Bounds.BoxExtent.Z);
    const double Bottom = (Bounds.Origin.Z - Bounds.BoxExtent.Z) * Scale;
    Body->SetRelativeScale3D(FVector(Scale));
    Body->SetRelativeLocation(FVector(0, 0, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - Bottom));
    Body->SetRelativeRotation(FRotator(0, -90, 0));
    if (UMaterialInterface* Skin = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Art/Crossroads/Survivor/M_StationZombie.M_StationZombie")))
    {
        for (int32 Slot = 0; Slot < Body->GetNumMaterials(); ++Slot)
        {
            Body->SetMaterial(Slot, Skin);
        }
    }
    if (UAnimSequence* Walk = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Art/Crossroads/Survivor/run.run")))
    {
        Body->PlayAnimation(Walk, true);
        Body->SetPlayRate(0.48f);
    }
}

bool ASurvivalZombie::ProbePath(const FVector& Destination, FHitResult& Hit) const
{
    const FVector Start = GetActorLocation();
    // Test at torso height so minor floor steps do not masquerade as a blocked route.
    return GetWorld()->SweepSingleByChannel(Hit, Start, FVector(Destination.X, Destination.Y, Start.Z),
        FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(GetCapsuleComponent()->GetScaledCapsuleRadius()),
        FCollisionQueryParams(SCENE_QUERY_STAT(StationZombie), false, this));
}

FVector ASurvivalZombie::FindDestination(const ASurvivalCharacter& Target) const
{
    const FVector Goal = Target.GetActorLocation();
    FHitResult Obstruction;
    if (!ProbePath(Goal, Obstruction) || Obstruction.GetActor() == &Target)
    {
        return Goal;
    }

    // The station shop has one entrance. Route around the permanent front wall through that opening.
    constexpr double DoorX = 3700.0;
    constexpr double DoorY = 4300.0;
    const bool bTargetInShop = Goal.X > 2825.0 && Goal.X < 4575.0 && Goal.Y > DoorY && Goal.Y < 5475.0;
    const FVector Position = GetActorLocation();
    const bool bZombieInShop = Position.X > 2825.0 && Position.X < 4575.0
        && Position.Y > DoorY && Position.Y < 5475.0;
    const double Side = bTargetInShop ? -1.0 : 1.0;
    if (bZombieInShop == bTargetInShop && FMath::Abs(Position.Y - DoorY) > 90.0)
    {
        return Goal;
    }
    const FVector Approach(DoorX, DoorY + Side * 105.0, Position.Z);
    if ((Position.Y - DoorY) * Side > 90.0 && FVector::DistSquared2D(Position, Approach) > FMath::Square(70.0))
    {
        return Approach;
    }
    return FVector(DoorX, DoorY - Side * 115.0, Position.Z);
}

void ASurvivalZombie::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    ASurvivalCharacter* Target = Cast<ASurvivalCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Target || Target->GetHealthPoints() <= 0 || Health <= 0)
    {
        GetMesh()->SetPlayRate(0.0f);
        return;
    }

    const FVector Destination = FindDestination(*Target);
    FHitResult Obstruction;
    const FVector Direction = (Destination - GetActorLocation()).GetSafeNormal2D();
    const bool bBlocked = ProbePath(GetActorLocation() + Direction * 115.0, Obstruction);
    ASurvivalBuildChunk* Chunk = bBlocked ? Cast<ASurvivalBuildChunk>(Obstruction.GetActor()) : nullptr;
    if ((Chunk && Chunk->FindFinished(Obstruction)) || (bBlocked && Obstruction.GetActor() == Target))
    {
        GetMesh()->SetPlayRate(0.0f);
        GetCharacterMovement()->StopMovementImmediately();
        const double Now = GetWorld()->GetTimeSeconds();
        if (Now >= NextAttackTime)
        {
            if (Chunk)
            {
                Chunk->DamageFinished(Obstruction, 18);
            }
            else
            {
                Target->ReceiveZombieHit(15);
            }
            NextAttackTime = Now + 1.35;
        }
        return;
    }

    if (!bBlocked)
    {
        GetMesh()->SetPlayRate(0.48f);
        AddMovementInput(Direction, 1.0f, /*bForce=*/true);
        return;
    }

    // Keep the station route usable with crates and pump columns in it.
    for (const double Turn : {-55.0, 55.0})
    {
        const FVector Detour = Direction.RotateAngleAxis(Turn, FVector::UpVector);
        if (!ProbePath(GetActorLocation() + Detour * 100.0, Obstruction))
        {
            GetMesh()->SetPlayRate(0.48f);
            AddMovementInput(Detour, 1.0f, /*bForce=*/true);
            return;
        }
    }
    GetMesh()->SetPlayRate(0.0f);
}

void ASurvivalZombie::HitBySidearm()
{
    Health -= 32;
    if (Health <= 0)
    {
        Destroy();
    }
}
