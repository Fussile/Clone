#include "SurvivalBuildChunk.h"

#include "CollisionQueryParams.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "SurvivalGameMode.h"
#include "UObject/ConstructorHelpers.h"

const FSurvivalBuildPlacement* FSurvivalPlanTarget::GetPlan() const
{
    return IsValid(Chunk) ? Chunk->GetPlan(PlanIndex) : nullptr;
}

ASurvivalBuildChunk::ASurvivalBuildChunk()
{
    Timber = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Timber"));
    SetRootComponent(Timber);
    Stone = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Stone"));
    Stone->SetupAttachment(Timber);
    Ghosts = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Ghosts"));
    Ghosts->SetupAttachment(Timber);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    for (UInstancedStaticMeshComponent* Component : {Timber.Get(), Stone.Get(), Ghosts.Get()})
    {
        Component->SetMobility(EComponentMobility::Movable);
        if (Cube.Succeeded())
        {
            Component->SetStaticMesh(Cube.Object);
        }
    }

    Timber->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Stone->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Ghosts->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Ghosts->SetCollisionResponseToAllChannels(ECR_Ignore);
    Ghosts->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
    Ghosts->SetGenerateOverlapEvents(false);
    // Give selection-only geometry its own object type so ordinary object queries skip it.
    Ghosts->SetCollisionObjectType(ECC_GameTraceChannel2);
    Ghosts->SetCanEverAffectNavigation(false);
    Ghosts->SetCastShadow(false);
}

void ASurvivalBuildChunk::BeginPlay()
{
    Super::BeginPlay();
    UMaterialInterface* TimberMaterial = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Art/Crossroads/Building/M_CrossroadsTimber.M_CrossroadsTimber"), nullptr, LOAD_NoWarn);
    if (!TimberMaterial)
    {
        TimberMaterial = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Art/Crossroads/Building/M_Timber.M_Timber"), nullptr, LOAD_NoWarn);
    }
    Timber->SetMaterial(0, TimberMaterial);
    Stone->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Art/Crossroads/Surfaces/M_Concrete034.M_Concrete034")));
    Ghosts->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Art/Crossroads/Building/M_BuildPlan.M_BuildPlan")));
}

bool ASurvivalBuildChunk::AddInstances(UInstancedStaticMeshComponent& Component,
    const FSurvivalBuildPlacement& Placement, TArray<int32>& Indices)
{
    if (!Component.GetStaticMesh())
    {
        return false;
    }
    constexpr double CubeHalfExtent = 50.0;
    const auto AddBox = [&](const FBox& Box)
    {
        const int32 Index = Component.AddInstance(FTransform(Placement.Rotation, Placement.WorldCenter(Box),
            Box.GetExtent() / CubeHalfExtent), /*bWorldSpace=*/true);
        if (Index == INDEX_NONE)
        {
            for (int32 Added = Indices.Num() - 1; Added >= 0; --Added)
            {
                Component.RemoveInstance(Indices[Added]);
            }
            return false;
        }
        Indices.Add(Index);
        return true;
    };
    for (const FBox& Box : Placement.GetPiece().Boxes)
    {
        if (!AddBox(Box))
        {
            return false;
        }
    }
    return true;
}

bool ASurvivalBuildChunk::AddPlan(const FSurvivalBuildPlacement& Placement)
{
    TArray<int32> Indices;
    if (!AddInstances(*Ghosts, Placement, Indices))
    {
        return false;
    }
    const int32 PlanIndex = Plans.Add(Placement);
    for (const int32 Index : Indices)
    {
        GhostOwners.Add(Index, PlanIndex);
    }
    return true;
}

void ASurvivalBuildChunk::RemovePlan(int32 Index)
{
    if (Plans.IsValidIndex(Index))
    {
        Plans.RemoveAtSwap(Index);
        RebuildGhosts();
    }
}

bool ASurvivalBuildChunk::FinishPlan(int32 Index, int32 StructureId)
{
    const FSurvivalBuildPlacement* Plan = GetPlan(Index);
    if (!Plan || !AddFinishedPiece(*Plan, StructureId))
    {
        return false;
    }
    RemovePlan(Index);
    return true;
}

namespace
{
FTransform DoorTransform(const FSurvivalBuildPlacement& Placement, double Degrees)
{
    const FBox Leaf = FSurvivalBuildPlacement::ClosedDoor();
    const FVector Center = Leaf.GetCenter();
    const FVector Hinge(Leaf.Min.X, Center.Y, Center.Z);
    const FQuat Swing = FRotator(0, Degrees, 0).Quaternion();
    return FTransform(Placement.Rotation * Swing,
        Placement.MeshOrigin() + Placement.Rotation.RotateVector(Hinge + Swing.RotateVector(Center - Hinge)),
        Leaf.GetExtent() / 50.0);
}
}

bool ASurvivalBuildChunk::TryInstallDoor(const FHitResult& Hit)
{
    const int32 Index = FindFinishedIndex(Hit);
    if (!Finished.IsValidIndex(Index) || !Finished[Index].Placement.GetPiece().bDoorFrame
        || Finished[Index].Door.IsValid() || !Timber->GetStaticMesh())
    {
        return false;
    }
    FSurvivalFinishedPiece& Piece = Finished[Index];
    const FTransform Transform = DoorTransform(Piece.Placement, 0.0);
    const FCollisionShape Shape = FCollisionShape::MakeBox(FSurvivalBuildPlacement::ClosedDoor().GetExtent() - FVector(0.25));
    if (GetWorld()->OverlapBlockingTestByChannel(Transform.GetLocation(), Transform.GetRotation(), ECC_Pawn, Shape))
    {
        return false;
    }

    UStaticMeshComponent* Leaf = NewObject<UStaticMeshComponent>(this);
    AddInstanceComponent(Leaf);
    Leaf->SetupAttachment(GetRootComponent());
    Leaf->SetMobility(EComponentMobility::Movable);
    Leaf->SetStaticMesh(Timber->GetStaticMesh());
    Leaf->SetMaterial(0, Timber->GetMaterial(0));
    Leaf->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Leaf->SetCanEverAffectNavigation(false);
    Leaf->SetWorldTransform(Transform);
    Leaf->RegisterComponent();
    Piece.Door = Leaf;
    Piece.DoorHealth = 55;
    Piece.bDoorOpen = false;
    return true;
}

void ASurvivalBuildChunk::RemoveDoorLeaf(FSurvivalFinishedPiece& Piece)
{
    if (UStaticMeshComponent* Leaf = Piece.Door.Get())
    {
        RemoveInstanceComponent(Leaf);
        Leaf->DestroyComponent();
    }
    Piece.Door.Reset();
    Piece.DoorHealth = 0;
    Piece.bDoorOpen = false;
}

bool ASurvivalBuildChunk::TryToggleDoor(const FHitResult& Hit)
{
    const int32 Index = FindFinishedIndex(Hit);
    if (!Finished.IsValidIndex(Index) || !Finished[Index].Door.IsValid())
    {
        return false;
    }
    FSurvivalFinishedPiece& Piece = Finished[Index];
    UStaticMeshComponent* Leaf = Piece.Door.Get();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(DoorSwing), false);
    Query.AddIgnoredComponent(Leaf);
    const FCollisionShape Shape = FCollisionShape::MakeBox(FSurvivalBuildPlacement::ClosedDoor().GetExtent() - FVector(0.25));
    for (int32 Step = 1; Step <= 18; ++Step)
    {
        const double Angle = (Piece.bDoorOpen ? 18 - Step : Step) * 5.0;
        const FTransform Pose = DoorTransform(Piece.Placement, Angle);
        if (GetWorld()->OverlapBlockingTestByChannel(Pose.GetLocation(), Pose.GetRotation(), ECC_Pawn, Shape, Query))
        {
            return false;
        }
    }
    Piece.bDoorOpen = !Piece.bDoorOpen;
    Leaf->SetWorldTransform(DoorTransform(Piece.Placement, Piece.bDoorOpen ? 90.0 : 0.0));
    return true;
}

bool ASurvivalBuildChunk::HasReservedCellConflict(const FSurvivalBuildPlacement& Placement, bool bIncludePlans) const
{
    for (const FSurvivalFinishedPiece& Piece : Finished)
    {
        if (Placement.OverlapsReservedCells(Piece.Placement))
        {
            return true;
        }
    }
    if (bIncludePlans)
    {
        for (const FSurvivalBuildPlacement& Plan : Plans)
        {
            if (Placement.OverlapsReservedCells(Plan))
            {
                return true;
            }
        }
    }
    return false;
}

bool ASurvivalBuildChunk::AddFinishedPiece(const FSurvivalBuildPlacement& Placement, int32 StructureId)
{
    const int32 MaxHealth = Placement.GetPiece().bStone ? 140 : 70;
    const int32 FinishedIndex = Finished.Add(FSurvivalFinishedPiece{Placement, MaxHealth, MaxHealth, StructureId});
    if (!AddFinishedInstances(FinishedIndex))
    {
        Finished.Pop();
        return false;
    }
    return true;
}

bool ASurvivalBuildChunk::AddFinishedInstances(int32 Index)
{
    const FSurvivalBuildPlacement& Placement = Finished[Index].Placement;
    const bool bStone = Placement.GetPiece().bStone;
    TArray<int32> Indices;
    if (!AddInstances(bStone ? *Stone : *Timber, Placement, Indices))
    {
        return false;
    }
    TMap<int32, int32>& Owners = bStone ? StoneOwners : TimberOwners;
    for (int32 Instance : Indices)
    {
        Owners.Add(Instance, Index);
    }
    return true;
}

int32 ASurvivalBuildChunk::FindFinishedIndex(const FHitResult& Hit) const
{
    const TMap<int32, int32>* Owners = nullptr;
    if (Hit.GetComponent() == Stone.Get())
    {
        Owners = &StoneOwners;
    }
    else if (Hit.GetComponent() == Timber.Get())
    {
        Owners = &TimberOwners;
    }
    if (Owners)
    {
        const int32* Index = Owners->Find(Hit.Item);
        return Index ? *Index : INDEX_NONE;
    }
    if (!Hit.GetComponent() || Hit.GetComponent() == Ghosts.Get())
    {
        return INDEX_NONE;
    }
    return Finished.IndexOfByPredicate([&Hit](const FSurvivalFinishedPiece& Piece)
    {
        return Piece.Door.Get() == Hit.GetComponent();
    });
}

const FSurvivalFinishedPiece* ASurvivalBuildChunk::FindFinished(const FHitResult& Hit) const
{
    const int32 Index = FindFinishedIndex(Hit);
    return Finished.IsValidIndex(Index) ? &Finished[Index] : nullptr;
}

bool ASurvivalBuildChunk::DamageFinished(const FHitResult& Hit, int32 Amount)
{
    const int32 Index = FindFinishedIndex(Hit);
    if (!Finished.IsValidIndex(Index) || Amount <= 0)
    {
        return false;
    }
    FSurvivalFinishedPiece& Piece = Finished[Index];
    if (Piece.Door.IsValid() && Hit.GetComponent() == Piece.Door.Get())
    {
        Piece.DoorHealth -= Amount;
        if (Piece.DoorHealth <= 0)
        {
            RemoveDoorLeaf(Piece);
        }
        return true;
    }
    if (Amount < Piece.Health)
    {
        Piece.Health -= Amount;
        return true;
    }
    if (ASurvivalGameMode* GameMode = GetWorld()->GetAuthGameMode<ASurvivalGameMode>())
    {
        GameMode->RemoveStructure(Piece.StructureId);
        return true;
    }
    return false;
}

void ASurvivalBuildChunk::RemoveFinishedPieces(const TSet<int32>& StructureIds, int32& DebrisSlots)
{
    TArray<FSurvivalBuildPlacement> Debris;
    Finished.RemoveAllSwap([&](FSurvivalFinishedPiece& Piece)
    {
        if (!StructureIds.Contains(Piece.StructureId))
        {
            return false;
        }
        RemoveDoorLeaf(Piece);
        if (DebrisSlots > 0)
        {
            --DebrisSlots;
            Debris.Add(Piece.Placement);
        }
        return true;
    });
    Timber->ClearInstances();
    Stone->ClearInstances();
    TimberOwners.Reset();
    StoneOwners.Reset();
    for (int32 Remaining = 0; Remaining < Finished.Num(); ++Remaining)
    {
        const bool bAdded = AddFinishedInstances(Remaining);
        ensure(bAdded);
    }
    for (const FSurvivalBuildPlacement& Placement : Debris)
    {
        SpawnDebris(Placement);
    }
}

void ASurvivalBuildChunk::SpawnDebris(const FSurvivalBuildPlacement& Placement)
{
    const UInstancedStaticMeshComponent& Source = Placement.GetPiece().bStone ? *Stone : *Timber;
    const FBox& Box = Placement.GetPiece().Boxes[0];
    AStaticMeshActor* DebrisActor = GetWorld()->SpawnActor<AStaticMeshActor>(
        Placement.WorldCenter(Box), Placement.Rotation.Rotator());
    if (!DebrisActor)
    {
        return;
    }
    UStaticMeshComponent* Fragment = DebrisActor->GetStaticMeshComponent();
    Fragment->SetMobility(EComponentMobility::Movable);
    Fragment->SetStaticMesh(Source.GetStaticMesh());
    Fragment->SetMaterial(0, Source.GetMaterial(0));
    DebrisActor->SetActorScale3D(Box.GetExtent() * (0.72 / 50.0));
    Fragment->SetCollisionProfileName(TEXT("PhysicsActor"));
    Fragment->SetCollisionResponseToAllChannels(ECR_Ignore);
    Fragment->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    Fragment->SetCanEverAffectNavigation(false);
    Fragment->SetSimulatePhysics(true);
    Fragment->AddAngularImpulseInDegrees(FVector(27, 20, 13), NAME_None, true);
    DebrisActor->SetLifeSpan(2.5f);
}

bool ASurvivalBuildChunk::RepairFinished(const FHitResult& Hit, FSurvivalMaterials& Inventory, int32 RepairHealth)
{
    const int32 Index = FindFinishedIndex(Hit);
    if (!Finished.IsValidIndex(Index))
    {
        return false;
    }
    FSurvivalFinishedPiece& Piece = Finished[Index];
    const FSurvivalMaterials Cost = Piece.RepairCost();
    if (Piece.Health >= Piece.MaxHealth || !Cost.MissingFrom(Inventory).IsEmpty())
    {
        return false;
    }
    Inventory.Spend(Cost);
    Piece.Health = FMath::Min(Piece.MaxHealth, Piece.Health + RepairHealth);
    return true;
}

const FSurvivalBuildPlacement* ASurvivalBuildChunk::GetPlan(int32 Index) const
{
    return Plans.IsValidIndex(Index) ? &Plans[Index] : nullptr;
}

FSurvivalPlanTarget ASurvivalBuildChunk::GetPlanTarget(const FHitResult& Hit)
{
    if (Hit.GetComponent() == Ghosts.Get())
    {
        if (const int32* Index = GhostOwners.Find(Hit.Item))
        {
            return {this, *Index};
        }
    }
    return {};
}

void ASurvivalBuildChunk::RebuildGhosts()
{
    Ghosts->ClearInstances();
    GhostOwners.Reset();
    for (int32 PlanIndex = 0; PlanIndex < Plans.Num(); ++PlanIndex)
    {
        TArray<int32> Indices;
        if (AddInstances(*Ghosts, Plans[PlanIndex], Indices))
        {
            for (const int32 Index : Indices)
            {
                GhostOwners.Add(Index, PlanIndex);
            }
        }
    }
}
