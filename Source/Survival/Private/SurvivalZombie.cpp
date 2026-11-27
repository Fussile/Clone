#include "SurvivalZombie.h"

#include "Animation/AnimSequence.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "SurvivalBuildChunk.h"
#include "SurvivalCharacter.h"
#include "SurvivalWorldDoor.h"
#include "SurvivalSupplyCrate.h"
#include "Engine/DamageEvents.h"

namespace
{
constexpr double AttackReach = 115.0;
constexpr double AttackInterval = 1.35;
constexpr double StrikeInSourceClip = 0.84;
constexpr int32 StructureDamage = 18;
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

ASurvivalZombie* ASurvivalZombie::SpawnOnGround(UWorld& World, const FVector& Around,
    EEncounterEnemy Kind, int32 Stage, bool bHorde)
{
    FHitResult Floor;
    const double Rise = bHorde ? 550.0 : 35.0;
    const double Drop = bHorde ? 1800.0 : 170.0;
    if (!World.LineTraceSingleByChannel(Floor, Around + FVector(0, 0, Rise),
            Around - FVector(0, 0, Drop), ECC_WorldStatic,
            FCollisionQueryParams(SCENE_QUERY_STAT(EncounterFloor), true))
        || Floor.ImpactNormal.Z < 0.7 || Cast<APawn>(Floor.GetActor()))
    {
        return nullptr;
    }

    const double HalfHeight = GetDefault<ASurvivalZombie>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FTransform Transform(FRotator(0, FMath::FRandRange(-180.f, 180.f), 0),
        Floor.ImpactPoint + FVector(0, 0, HalfHeight + 3.0));
    ASurvivalZombie* Enemy = World.SpawnActorDeferred<ASurvivalZombie>(StaticClass(), Transform,
        nullptr, nullptr, ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding);
    if (Enemy)
    {
        Enemy->EnemyKind = Kind;
        Enemy->bIsRunner = Kind == EEncounterEnemy::Runner || Kind == EEncounterEnemy::Hound;
        Enemy->StageBonus = FMath::Clamp(Stage - 1, 0, 7);
        Enemy->AggroRange = bHorde ? 0.f : Kind == EEncounterEnemy::Hound ? 3000.f :
            Kind == EEncounterEnemy::Toxic ? 1400.f : 2100.f;
        Enemy->FinishSpawning(Transform);
    }
    return IsValid(Enemy) ? Enemy : nullptr;
}

void ASurvivalZombie::BeginPlay()
{
    Super::BeginPlay();
    Home = GetActorLocation();
    if (bIsRunner)
    {
        GetCharacterMovement()->MaxWalkSpeed = 430.0f;
        Health = 55;
    }
    const bool bHound = EnemyKind == EEncounterEnemy::Hound;
    const bool bToxic = EnemyKind == EEncounterEnemy::Toxic;
    Health = (bHound ? 45 : bToxic ? 120 : Health) + 5 * StageBonus;
    ContactDamage = (bHound ? 10 : bToxic ? 20 : 15) + StageBonus;
    if (bHound || bToxic)
    {
        const FString VariantName = bHound ? TEXT("Hound") : TEXT("Toxic");
        const FString Folder = TEXT("/Game/Art/Crossroads/Enemies/") + VariantName + TEXT("/");
        const FString MeshName = TEXT("SK_") + VariantName;
        const FString ClipName = VariantName + TEXT("Loop");
        USkeletalMesh* Variant = LoadObject<USkeletalMesh>(nullptr, *(Folder + MeshName + TEXT(".") + MeshName));
        UAnimSequence* Clip = LoadObject<UAnimSequence>(nullptr, *(Folder + ClipName + TEXT(".") + ClipName));
        if (Variant && Clip)
        {
            UCapsuleComponent* Capsule = GetCapsuleComponent();
            if (bHound)
            {
                const float OldHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
                Capsule->SetCapsuleSize(32.f, 44.f);
                AddActorWorldOffset(FVector(0, 0, 44.f - OldHalfHeight));
                Home = GetActorLocation();
            }

            const FBox Bounds = Variant->GetBounds().GetBox();
            const double Scale = (bHound ? 85.0 : 180.0) / FMath::Max(1.0, Bounds.GetSize().Z);
            GetMesh()->SetSkeletalMesh(Variant);
            GetMesh()->SetRelativeScale3D(FVector(Scale));
            GetMesh()->SetRelativeRotation(FRotator(0, -90, 0));
            GetMesh()->SetRelativeLocation(FVector(0, 0, -Capsule->GetUnscaledCapsuleHalfHeight() - Scale * Bounds.Min.Z));
            IdleAnimation = Clip;
            WalkAnimation = Clip;
            GetCharacterMovement()->MaxWalkSpeed = bHound ? 510.f : 185.f;
            UpdateLocomotion(false);
            return;
        }
    }

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
    const float Pace = bMoving && bIsRunner ? 1.65f : 1.0f;
    GetMesh()->SetPlayRate((IdleAnimation ? 1.0f : 0.48f) * Pace);
}

void ASurvivalZombie::ApplyAttack()
{
    FHitResult Hit;
    if (!ProbePath(GetActorLocation() + AttackDirection * AttackReach, Hit))
    {
        return;
    }
    if (ASurvivalBuildChunk* Chunk = Cast<ASurvivalBuildChunk>(Hit.GetActor()))
    {
        Chunk->DamageFinished(Hit, StructureDamage);
    }
    else if (ASurvivalWorldDoor* Door = Cast<ASurvivalWorldDoor>(Hit.GetActor()))
    {
        Door->TakeBreachDamage(StructureDamage);
    }
    else if (ASurvivalSupplyCrate* Prop = Cast<ASurvivalSupplyCrate>(Hit.GetActor()))
    {
        Prop->TakeDamage(StructureDamage, FDamageEvent{}, GetController(), this);
    }
    else if (ASurvivalCharacter* Target = Cast<ASurvivalCharacter>(Hit.GetActor()))
    {
        Target->TakeSurvivalDamage(ContactDamage);
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

void ASurvivalZombie::RememberTarget(const ASurvivalCharacter& Target, double Now)
{
    LastKnownLocation = Target.GetActorLocation();
    SearchUntil = Now + 8.0;
}

void ASurvivalZombie::UpdatePerception(double Now)
{
    if (Now < NextSenseTime)
    {
        return;
    }
    NextSenseTime = Now + 0.2;
    const ASurvivalCharacter* Nearest = nullptr;
    const double Range = AggroRange > 0.0f ? AggroRange : 10000.0;
    double BestDistance = FMath::Square(Range);
    for (TActorIterator<ASurvivalCharacter> It(GetWorld()); It; ++It)
    {
        if (It->GetHealthPoints() <= 0)
        {
            continue;
        }
        const double Distance = FVector::DistSquared(GetActorLocation(), It->GetActorLocation());
        if (Distance > BestDistance)
        {
            continue;
        }
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ZombiePerception), false, this);
        Query.AddIgnoredActor(*It);
        const FVector Eyes = GetActorLocation() + FVector(0, 0, 50);
        const FVector TargetPoint = It->GetActorLocation() + FVector(0, 0, 20);
        const double MovementNoise = It->GetVelocity().Size2D() > 650.0 ? 1300.0 : 550.0;
        const double HearingRange = EnemyKind == EEncounterEnemy::Hound ? 2.0 * MovementNoise : MovementNoise;
        const bool bHeard = It->GetVelocity().SizeSquared2D() > FMath::Square(150.0)
            && Distance <= FMath::Square(HearingRange);
        const bool bSeen = !GetWorld()->LineTraceTestByChannel(Eyes, TargetPoint, ECC_Visibility, Query);
        if (AggroRange <= 0.0f || bHeard || bSeen)
        {
            Nearest = *It;
            BestDistance = Distance;
        }
    }
    if (Nearest)
    {
        RememberTarget(*Nearest, Now);
    }
}

FVector ASurvivalZombie::ChooseMoveDirection(const FVector& Desired)
{
    FHitResult Obstruction;
    for (const double Angle : {40.0, 70.0, 100.0, 135.0})
    {
        for (const double Side : {DetourSide, -DetourSide})
        {
            const FVector Option = Desired.RotateAngleAxis(Angle * Side, FVector::UpVector);
            if (!ProbePath(GetActorLocation() + Option * 105.0, Obstruction))
            {
                DetourSide = Side;
                return Option;
            }
        }
    }
    return FVector::ZeroVector;
}

void ASurvivalZombie::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (Health <= 0)
    {
        return;
    }
    const double Now = GetWorld()->GetTimeSeconds();
    UpdatePerception(Now);
    if (AttackImpactTime >= 0.0 && Now >= AttackImpactTime)
    {
        AttackImpactTime = -1.0;
        ApplyAttack();
    }
    if (AttackAnimation && Now < NextAttackTime)
    {
        return;
    }

    const bool bTracking = Now < SearchUntil;
    const FVector Destination = bTracking ? LastKnownLocation : Home;
    if (FVector::DistSquared2D(Destination, GetActorLocation()) < FMath::Square(bTracking ? 25.0 : 75.0))
    {
        UpdateLocomotion(false);
        return;
    }
    FHitResult Obstruction;
    const FVector Direction = (Destination - GetActorLocation()).GetSafeNormal2D();
    const bool bBlocked = ProbePath(GetActorLocation() + Direction * AttackReach, Obstruction);
    AActor* Blocker = bBlocked ? Obstruction.GetActor() : nullptr;
    const ASurvivalBuildChunk* Chunk = Cast<ASurvivalBuildChunk>(Blocker);
    const ASurvivalWorldDoor* Door = Cast<ASurvivalWorldDoor>(Blocker);
    if (bTracking && ((Chunk && Chunk->FindFinished(Obstruction))
        || (Door && !Door->IsBroken()) || Cast<ASurvivalSupplyCrate>(Blocker)
        || Cast<ASurvivalCharacter>(Blocker)))
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
                ApplyAttack();
            }
        }
        return;
    }

    const FVector Move = bBlocked ? ChooseMoveDirection(Direction) : Direction;
    UpdateLocomotion(!Move.IsNearlyZero());
    if (!Move.IsNearlyZero())
    {
        AddMovementInput(Move, 1.0f, /*bForce=*/true);
    }
}

void ASurvivalZombie::HitByShot(ASurvivalCharacter* Shooter, int32 Damage)
{
    if (Shooter)
    {
        RememberTarget(*Shooter, GetWorld()->GetTimeSeconds());
    }
    Health -= Damage;
    if (Health <= 0)
    {
        Destroy();
    }
}
