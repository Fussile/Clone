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
#include "SurvivalBuildChunk.h"
#include "SurvivalCharacter.h"

namespace
{
constexpr double AttackReach = 115.0;
constexpr double AttackInterval = 1.35;
constexpr double StrikeInSourceClip = 0.84;
}

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
        TEXT("/Game/Art/Crossroads/StationZombie/SK_StationZombie.SK_StationZombie"), nullptr, LOAD_NoWarn);
    if (!ZombieAsset)
    {
        return;
    }
    USkeletalMeshComponent* Body = GetMesh();
    Body->SetSkeletalMesh(ZombieAsset);
    // The imported asset's static bounds can describe the unposed mesh in a different basis.
    // Use the prepared GLB's measured skinned reference height and floor in Unreal centimeters.
    const double ReferenceHeight = 131.1;
    const double ReferenceBottom = 1.3;
    const double Scale = 175.0 / ReferenceHeight;
    const double Bottom = ReferenceBottom * Scale;
    Body->SetRelativeScale3D(FVector(Scale));
    Body->SetRelativeLocation(FVector(0, 0, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - Bottom));
    Body->SetRelativeRotation(FRotator(0, -90, 0));
    IdleAnimation = LoadObject<UAnimSequence>(nullptr,
        TEXT("/Game/Art/Crossroads/StationZombie/StationZombieIdle.StationZombieIdle"));
    WalkAnimation = LoadObject<UAnimSequence>(nullptr,
        TEXT("/Game/Art/Crossroads/StationZombie/StationZombieWalk.StationZombieWalk"));
    AttackAnimation = LoadObject<UAnimSequence>(nullptr,
        TEXT("/Game/Art/Crossroads/StationZombie/StationZombieAttack.StationZombieAttack"));
    UpdateLocomotion(false);
}

void ASurvivalZombie::UpdateLocomotion(bool bMoving)
{
    UAnimSequence* Sequence = bMoving && WalkAnimation ? WalkAnimation.Get() : IdleAnimation.Get();
    if (!Sequence)
    {
        GetMesh()->SetPlayRate(0.0f);
        return;
    }
    if (ActiveAnimation.Get() != Sequence)
    {
        ActiveAnimation = Sequence;
        GetMesh()->PlayAnimation(Sequence, true);
    }
    GetMesh()->SetPlayRate(IdleAnimation ? 1.0f : 0.48f);
}

void ASurvivalZombie::ApplyAttack(ASurvivalCharacter& Target)
{
    FHitResult Hit;
    if (!ProbePath(GetActorLocation() + AttackDirection * AttackReach, Hit))
    {
        return;
    }
    if (ASurvivalBuildChunk* Chunk = Cast<ASurvivalBuildChunk>(Hit.GetActor()))
    {
        Chunk->DamageFinished(Hit, 18);
    }
    else if (Hit.GetActor() == &Target)
    {
        Target.TakeSurvivalDamage(15);
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

    if (!bAlerted && AggroRange > 0.0f
        && FVector::DistSquared2D(GetActorLocation(), Target->GetActorLocation()) > FMath::Square(AggroRange))
    {
        UpdateLocomotion(false);
        return;
    }
    bAlerted = true;

    const double Now = GetWorld()->GetTimeSeconds();
    if (AttackImpactTime >= 0.0 && Now >= AttackImpactTime)
    {
        AttackImpactTime = -1.0;
        ApplyAttack(*Target);
    }
    if (AttackAnimation && Now < NextAttackTime)
    {
        return;
    }

    const FVector Destination = FindDestination(*Target);
    FHitResult Obstruction;
    const FVector Direction = (Destination - GetActorLocation()).GetSafeNormal2D();
    const bool bBlocked = ProbePath(GetActorLocation() + Direction * AttackReach, Obstruction);
    ASurvivalBuildChunk* Chunk = bBlocked ? Cast<ASurvivalBuildChunk>(Obstruction.GetActor()) : nullptr;
    if ((Chunk && Chunk->FindFinished(Obstruction)) || (bBlocked && Obstruction.GetActor() == Target))
    {
        GetCharacterMovement()->StopMovementImmediately();
        if (Now >= NextAttackTime)
        {
            AttackDirection = Direction;
            NextAttackTime = Now + AttackInterval;
            if (AttackAnimation)
            {
                SetActorRotation(Direction.Rotation());
                const double ClipLength = AttackAnimation->GetPlayLength();
                const double Rate = FMath::Max(1.0, ClipLength / AttackInterval);
                ActiveAnimation = AttackAnimation;
                GetMesh()->PlayAnimation(AttackAnimation.Get(), false);
                GetMesh()->SetPlayRate(static_cast<float>(Rate));
                AttackImpactTime = Now + FMath::Min(StrikeInSourceClip, ClipLength) / Rate;
            }
            else
            {
                UpdateLocomotion(false);
                ApplyAttack(*Target);
            }
        }
        return;
    }

    if (!bBlocked)
    {
        UpdateLocomotion(true);
        AddMovementInput(Direction, 1.0f, /*bForce=*/true);
        return;
    }

    // Keep the station route usable with crates and pump columns in it.
    for (const double Turn : {-55.0, 55.0})
    {
        const FVector Detour = Direction.RotateAngleAxis(Turn, FVector::UpVector);
        if (!ProbePath(GetActorLocation() + Detour * 100.0, Obstruction))
        {
            UpdateLocomotion(true);
            AddMovementInput(Detour, 1.0f, /*bForce=*/true);
            return;
        }
    }
    UpdateLocomotion(false);
}

void ASurvivalZombie::HitBySidearm()
{
    bAlerted = true;
    Health -= 32;
    if (Health <= 0)
    {
        Destroy();
    }
}
