#include "SurvivalBuildChunk.h"

#include "CollisionQueryParams.h"
#include "Net/UnrealNetwork.h"
#include "Materials/MaterialInstanceDynamic.h"
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
    bReplicates = true;
    Timber = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Timber"));
    SetRootComponent(Timber);
    Stone = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Stone"));
    Stone->SetupAttachment(Timber);
    Metal = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ConstructionMetal"));
    Metal->SetupAttachment(Timber);
    Metal->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Bracing = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Reinforcement"));
    Bracing->SetupAttachment(Timber);
    Bracing->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Ghosts = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Ghosts"));
    Ghosts->SetupAttachment(Timber);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    for (UInstancedStaticMeshComponent* Component : {Timber.Get(), Stone.Get(), Metal.Get(), Bracing.Get(), Ghosts.Get()})
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
    if (auto* Mat = Metal->CreateDynamicMaterialInstance(0))
        Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(.24f,.27f,.28f));
    if (auto* Mat = Bracing->CreateDynamicMaterialInstance(0))
        Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(.12f,.14f,.15f));
    Stone->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Art/Crossroads/Surfaces/M_Concrete034.M_Concrete034")));
    Ghosts->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Art/Crossroads/Building/M_BuildPlan.M_BuildPlan")));
}

bool ASurvivalBuildChunk::AddInstances(UInstancedStaticMeshComponent& Component,
    const FSurvivalBuildPlacement& Placement, TArray<int32>& Indices, bool bFrame)
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
        if (bFrame)
        {
            // Outline each collision box so stairs and door frames retain their actual shape.
            const FVector Beam = Box.GetSize().ComponentMin(FVector(8.));
            if ((Box.GetSize().X <= 8. && Box.GetSize().Y <= 8.) ||
                (Box.GetSize().X <= 8. && Box.GetSize().Z <= 8.) || (Box.GetSize().Z <= 8. && Box.GetSize().Y <= 8.))
            {
                if (!AddBox(Box)) return false;
                continue;
            }
            for (int32 Axis = 0; Axis < 3; ++Axis)
            {
                const int32 U = (Axis + 1) % 3, V = (Axis + 2) % 3;
                for (int32 SideU = 0; SideU < (Box.GetSize()[U] <= 8. ? 1 : 2); ++SideU)
                    for (int32 SideV = 0; SideV < (Box.GetSize()[V] <= 8. ? 1 : 2); ++SideV)
                    {
                        FVector Low = Box.Min, High = Box.Max;
                        if (SideU) Low[U] = High[U] - Beam[U]; else High[U] = Low[U] + Beam[U];
                        if (SideV) Low[V] = High[V] - Beam[V]; else High[V] = Low[V] + Beam[V];
                        if (!AddBox(FBox(Low, High))) return false;
                    }
            }
            continue;
        }
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
    if (!Plan || !AddFinishedPiece(*Plan, StructureId, !Plan->GetPiece().bStone))
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

    Piece.DoorHealth = 55;
    Piece.bDoorOpen = false;
    CreateDoorLeaf(Piece);
    if (Piece.Tier != INDEX_NONE) PublishPlayerPieces();
    return true;
}

void ASurvivalBuildChunk::CreateDoorLeaf(FSurvivalFinishedPiece& Piece)
{
    UStaticMeshComponent* Leaf = NewObject<UStaticMeshComponent>(this);
    AddInstanceComponent(Leaf);
    Leaf->SetupAttachment(GetRootComponent());
    Leaf->SetMobility(EComponentMobility::Movable);
    UStaticMesh* DoorArt = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Game/Art/Crossroads/World/Parish/SM_TimberLeaf/SM_TimberLeaf.SM_TimberLeaf"));
    UStaticMesh* LeafMesh = Timber->GetStaticMesh();
    Leaf->SetStaticMesh(DoorArt ? DoorArt : LeafMesh);
    if (!DoorArt)
    {
        Leaf->SetMaterial(0, Timber->GetMaterial(0));
    }
    Leaf->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Leaf->SetCanEverAffectNavigation(false);
    Leaf->SetWorldTransform(DoorTransform(Piece.Placement, Piece.bDoorOpen ? (Piece.bSwingNegative ? -90. : 90.) : 0.));
    Leaf->RegisterComponent();
    Piece.Door = Leaf;
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

bool ASurvivalBuildChunk::TryToggleDoor(const FHitResult& Hit, const FVector& UserLocation)
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
    const FTransform Closed = DoorTransform(Piece.Placement, 0.0);
    const bool bNegative = Piece.bDoorOpen ? Piece.bSwingNegative : Closed.InverseTransformPosition(UserLocation).Y > 0;
    const double SwingSign = bNegative ? -1.0 : 1.0;
    for (int32 Step = 1; Step <= 18; ++Step)
    {
        const double Angle = (Piece.bDoorOpen ? 18 - Step : Step) * 5.0 * SwingSign;
        const FTransform Pose = DoorTransform(Piece.Placement, Angle);
        if (GetWorld()->OverlapBlockingTestByChannel(Pose.GetLocation(), Pose.GetRotation(), ECC_Pawn, Shape, Query))
        {
            return false;
        }
    }
    Piece.bSwingNegative = bNegative;
    Piece.bDoorOpen = !Piece.bDoorOpen;
    Leaf->SetWorldTransform(DoorTransform(Piece.Placement, Piece.bDoorOpen ? SwingSign * 90.0 : 0.0));
    if (Piece.Tier != INDEX_NONE) PublishPlayerPieces();
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

bool ASurvivalBuildChunk::AddFinishedPiece(const FSurvivalBuildPlacement& Placement, int32 StructureId, bool bFrame)
{
    const int32 MaxHealth = bFrame ? 35 : Placement.GetPiece().bStone ? 140 : 70;
    const int32 FinishedIndex = Finished.Add(FSurvivalFinishedPiece{Placement, MaxHealth, MaxHealth, StructureId});
    Finished[FinishedIndex].Tier = bFrame ? 0 : INDEX_NONE;
    if (!AddFinishedInstances(FinishedIndex))
    {
        Finished.Pop();
        return false;
    }
    if (bFrame) PublishPlayerPieces();
    return true;
}

bool ASurvivalBuildChunk::AddFinishedInstances(int32 Index)
{
    const FSurvivalBuildPlacement& Placement = Finished[Index].Placement;
    const bool bStone = Placement.GetPiece().bStone;
    TArray<int32> Indices;
    const int32 Tier = Finished[Index].Tier;
    if (!AddInstances(Tier >= 3 ? *Metal : bStone ? *Stone : *Timber, Placement, Indices, Tier == 0))
    {
        return false;
    }
    TMap<int32, int32>& Owners = Tier >= 3 ? MetalOwners : bStone ? StoneOwners : TimberOwners;
    for (int32 Instance : Indices)
    {
        Owners.Add(Instance, Index);
    }
    // Straps follow solid boxes, including lintels and stair treads, leaving openings clear.
    if (Tier == 2 || Tier >= 4)
        for (const FBox& Box : Placement.GetPiece().Boxes)
        {
            const int32 Count = Tier == 5 ? 3 : Tier == 4 ? 2 : 1;
            for (int32 Band = 1; Band <= Count; ++Band)
            {
                FVector Size = Box.GetSize() + FVector(1.);
                Size.X = FMath::Min(5., Size.X);
                FVector Center = Box.GetCenter();
                Center.X = FMath::Lerp(Box.Min.X, Box.Max.X, double(Band)/(Count+1));
                const FTransform Pose(Placement.Rotation, Placement.MeshOrigin() + Placement.Rotation.RotateVector(Center), Size / 100.);
                BraceOwners.Add(Bracing->AddInstance(Pose, true), Index);
            }
        }
    return true;
}

int32 ASurvivalBuildChunk::FindFinishedIndex(const FHitResult& Hit) const
{
    const TMap<int32, int32>* Owners = nullptr;
    if (Hit.GetComponent() == Bracing.Get()) Owners = &BraceOwners;
    else if (Hit.GetComponent() == Metal.Get()) Owners = &MetalOwners;
    else if (Hit.GetComponent() == Stone.Get())
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
        if (Piece.Tier != INDEX_NONE) PublishPlayerPieces();
        return true;
    }
    if (Amount < Piece.Health)
    {
        Piece.Health -= Amount;
        if (Piece.Tier != INDEX_NONE) PublishPlayerPieces();
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
    RebuildSolids();
    PublishPlayerPieces();
    for (const auto& Placement : Debris) SpawnDebris(Placement);
}

void ASurvivalBuildChunk::RebuildSolids()
{
    Bracing->ClearInstances();
    BraceOwners.Reset();
    Metal->ClearInstances();
    MetalOwners.Reset();
    Timber->ClearInstances();
    Stone->ClearInstances();
    TimberOwners.Reset();
    StoneOwners.Reset();
    for (int32 Remaining = 0; Remaining < Finished.Num(); ++Remaining)
    {
        const bool bAdded = AddFinishedInstances(Remaining);
        ensure(bAdded);
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
    if (Piece.Tier != INDEX_NONE) PublishPlayerPieces();
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

namespace
{
constexpr FSurvivalBuildTier BuildTiers[] = {
    {TEXT("Wood frame"), ESurvivalItem::Timber, 0, 35},
    {TEXT("Wood infill"), ESurvivalItem::Timber, 4, 70},
    {TEXT("Reinforced wood"), ESurvivalItem::Timber, 8, 120},
    {TEXT("Scrap metal"), ESurvivalItem::ScrapIron, 6, 200},
    {TEXT("Iron"), ESurvivalItem::IronIngot, 3, 320},
    {TEXT("Forged steel"), ESurvivalItem::Steel, 3, 520},
};
}

const FSurvivalBuildTier* FSurvivalFinishedPiece::NextTier() const
{
    return Tier >= 0 && Tier + 1 < UE_ARRAY_COUNT(BuildTiers) ? &BuildTiers[Tier + 1] : nullptr;
}

FString FSurvivalFinishedPiece::UpgradePrompt() const
{
    if (const auto* Next = NextTier())
        return FString::Printf(TEXT("%s  [RMB] %s  (%d %s)"), BuildTiers[Tier].Name, Next->Name, Next->Cost, SurvivalItemName(Next->Material));
    return Tier == INDEX_NONE ? FString() : FString(TEXT("Forged steel (fully upgraded)"));
}

bool ASurvivalBuildChunk::Upgrade(const FHitResult& Hit, int32 ExpectedId, int32 ExpectedTier)
{
    const int32 Index = FindFinishedIndex(Hit);
    if (!HasAuthority() || !Finished.IsValidIndex(Index)) return false;
    auto& Piece = Finished[Index];
    const auto* Next = Piece.NextTier();
    if (!Next || Piece.StructureId != ExpectedId || Piece.Tier != ExpectedTier) return false;
    if (Piece.Tier == 0)
        for (const FBox& Box : Piece.Placement.GetPiece().Boxes)
            if (GetWorld()->OverlapAnyTestByObjectType(Piece.Placement.WorldCenter(Box), Piece.Placement.Rotation,
                FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeBox(Box.GetExtent() - FVector(.1)))) return false;
    ++Piece.Tier;
    Piece.Health = Piece.MaxHealth = Next->HitPoints;
    RebuildSolids();
    PublishPlayerPieces();
    return true;
}

void ASurvivalBuildChunk::PublishPlayerPieces()
{
    PlayerPieces.Reset();
    for (const auto& Piece : Finished)
    {
        if (Piece.Tier == INDEX_NONE) continue;
        auto& State = PlayerPieces.AddDefaulted_GetRef();
        State.Base = Piece.Placement.Base;
        State.Offset = Piece.Placement.FrameOffset;
        State.Rotation = Piece.Placement.Rotation;
        State.PieceIndex = Piece.Placement.PieceIndex;
        State.Id = Piece.StructureId;
        State.Tier = Piece.Tier;
        State.Health = Piece.Health;
        State.DoorHealth = Piece.DoorHealth;
        State.bOpen = Piece.bDoorOpen;
        State.bNegative = Piece.bSwingNegative;
    }
    ForceNetUpdate();
}

void ASurvivalBuildChunk::ReceivePlayerPieces()
{
    for (auto& Piece : Finished) RemoveDoorLeaf(Piece);
    Finished.Reset();
    for (const auto& State : PlayerPieces)
    {
        FSurvivalBuildPlacement P;
        P.Base = State.Base;
        P.FrameOffset = State.Offset;
        P.Rotation = State.Rotation;
        P.PieceIndex = State.PieceIndex;
        const int32 Index = Finished.Add(FSurvivalFinishedPiece{P, State.Health, BuildTiers[State.Tier].HitPoints, State.Id});
        auto& Piece = Finished[Index];
        Piece.Tier = State.Tier;
        Piece.DoorHealth = State.DoorHealth;
        Piece.bDoorOpen = State.bOpen;
        Piece.bSwingNegative = State.bNegative;
        if (Piece.DoorHealth > 0) CreateDoorLeaf(Piece);
    }
    RebuildSolids();
}

void ASurvivalBuildChunk::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Props) const
{
    Super::GetLifetimeReplicatedProps(Props);
    DOREPLIFETIME(ASurvivalBuildChunk, PlayerPieces);
    DOREPLIFETIME(ASurvivalBuildChunk, Plans);
}
