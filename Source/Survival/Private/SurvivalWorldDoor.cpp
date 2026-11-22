#include "SurvivalWorldDoor.h"

#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ASurvivalWorldDoor::ASurvivalWorldDoor()
{
    bReplicates = true;
    Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("DoorOrigin"));
    SetRootComponent(Pivot);

    Leaf = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorLeaf"));
    Leaf->SetupAttachment(Pivot);
    Leaf->SetMobility(EComponentMobility::Movable);
    Leaf->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Leaf->SetGenerateOverlapEvents(false);

    Handle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorHandle"));
    Handle->SetupAttachment(Pivot);
    Handle->SetMobility(EComponentMobility::Movable);
    Handle->SetCollisionProfileName(TEXT("NoCollision"));
    Handle->SetCanEverAffectNavigation(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cube.Succeeded())
    {
        Leaf->SetStaticMesh(Cube.Object);
        Handle->SetStaticMesh(Cube.Object);
    }

    // Keep the jamb interactable when the leaf is open.
    JambTarget = CreateDefaultSubobject<UBoxComponent>(TEXT("JambInteraction"));
    JambTarget->SetupAttachment(Pivot);
    JambTarget->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    JambTarget->SetCollisionResponseToAllChannels(ECR_Ignore);
    JambTarget->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    JambTarget->SetGenerateOverlapEvents(false);
    JambTarget->SetCanEverAffectNavigation(false);
    JambTarget->SetHiddenInGame(true);
}

void ASurvivalWorldDoor::ConfigureDoor(double OpeningWidth, double OpeningHeight, bool bRollUpDoor,
    bool bReverseDoor, UMaterialInterface* Finish, UMaterialInterface* Hardware)
{
#if WITH_EDITOR
    Modify();
    Leaf->Modify();
    Handle->Modify();
#endif
    Width = FMath::Max(85.0, OpeningWidth);
    Height = FMath::Max(195.0, OpeningHeight);
    bRollUp = bRollUpDoor;
    bReverse = bReverseDoor;
    Leaf->SetMaterial(0, Finish);
    Handle->SetMaterial(0, Hardware);
    ApplySize();
}

void ASurvivalWorldDoor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ApplySize();
}

void ASurvivalWorldDoor::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority())
    {
        Durability = bRollUp ? 126 : 72;
    }
    ApplySize();
}

void ASurvivalWorldDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASurvivalWorldDoor, bOpen);
    DOREPLIFETIME(ASurvivalWorldDoor, bBroken);
}

FTransform ASurvivalWorldDoor::LeafPose(double Fraction) const
{
    const double LeafWidth = Width - 4.0;
    if (bRollUp)
    {
        const double LeafHeight = FMath::Lerp(Height - 4.0, 16.0, Fraction);
        return FTransform(FQuat::Identity, FVector(0.0, 0.0, Height - 2.0 - LeafHeight / 2.0),
            FVector(LeafWidth, 7.0, LeafHeight) / 100.0);
    }
    const double Side = bReverse ? 1.0 : -1.0;
    const FVector Center(0.0, 0.0, Height / 2.0);
    const FQuat Swing = FRotator(0, -Side * Fraction * 90.0, 0).Quaternion();
    const FVector Hinge(Side * LeafWidth / 2.0, 0.0, Center.Z);
    return FTransform(Swing, Hinge + Swing.RotateVector(Center - Hinge),
        FVector(LeafWidth, 5.0, Height - 4.0) / 100.0);
}

void ASurvivalWorldDoor::ApplySize()
{
    JambTarget->SetBoxExtent(FVector(7.0, 11.0, (Height - 8.0) / 2.0));
    JambTarget->SetRelativeLocation(FVector((bReverse ? 1.0 : -1.0) * (Width / 2.0 - 4.0), 0.0, Height / 2.0));
    ApplyDoorState();
}

void ASurvivalWorldDoor::ApplyDoorState()
{
    if (bBroken)
    {
        Leaf->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Leaf->SetVisibility(false);
        Handle->SetVisibility(false);
        JambTarget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        return;
    }

    const FTransform Pose = LeafPose(bOpen ? 1.0 : 0.0);
    Leaf->SetRelativeTransform(Pose);
    Handle->SetVisibility(!bRollUp || !bOpen);
    const double HandleX = bRollUp ? 0.0 : (bReverse ? -1.0 : 1.0) * (Width / 2.0 - 13.0);
    const FVector HandleOffset(HandleX, 0.0, FMath::Min(105.0, Height / 2.0) - Height / 2.0);
    Handle->SetRelativeTransform(FTransform(Pose.GetRotation(),
        Pose.TransformPositionNoScale(HandleOffset), FVector(.13, .15, .035)));
}

bool ASurvivalWorldDoor::TryToggle()
{
    if (!HasAuthority() || bBroken || !Leaf->GetStaticMesh())
    {
        return false;
    }

    const FCollisionQueryParams Query(SCENE_QUERY_STAT(WorldDoorClearance), false, this);
    const FCollisionObjectQueryParams Pawns(ECC_Pawn);
    const int32 Steps = FMath::Max(18, FMath::CeilToInt((bRollUp ? Height : Width) / 8.0));
    for (int32 Step = 1; Step <= Steps; ++Step)
    {
        const double Fraction = static_cast<double>(Step) / Steps;
        const FTransform Pose = LeafPose(bOpen ? 1.0 - Fraction : Fraction) * GetActorTransform();
        const FCollisionShape Shape = FCollisionShape::MakeBox(Pose.GetScale3D().GetAbs() * 50.0);
        if (GetWorld()->OverlapAnyTestByObjectType(Pose.GetLocation(), Pose.GetRotation(), Pawns, Shape, Query))
        {
            return false;
        }
    }
    bOpen = !bOpen;
    ApplyDoorState();
    ForceNetUpdate();
    return true;
}


void ASurvivalWorldDoor::TakeBreachDamage(int32 Amount)
{
    if (!HasAuthority() || bBroken || Amount <= 0)
    {
        return;
    }

    Durability = FMath::Max(0, Durability - Amount);
    if (Durability == 0)
    {
        bBroken = true;
        ApplyDoorState();
        ForceNetUpdate();
    }
}
