#include "SurvivalBuildChunk.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
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
    Timber->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Art/Crossroads/Building/M_Timber.M_Timber")));
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
    for (const FBox& Box : Placement.GetPiece().Boxes)
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

bool ASurvivalBuildChunk::FinishPlan(int32 Index)
{
    const FSurvivalBuildPlacement* Plan = GetPlan(Index);
    if (!Plan)
    {
        return false;
    }

    TArray<int32> Indices;
    UInstancedStaticMeshComponent& Component = Plan->GetPiece().bStone ? *Stone : *Timber;
    if (!AddInstances(Component, *Plan, Indices))
    {
        return false;
    }
    RemovePlan(Index);
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
