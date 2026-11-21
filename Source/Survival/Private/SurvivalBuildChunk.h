#pragma once

#include "GameFramework/Actor.h"
#include "SurvivalBuilding.h"
#include "SurvivalBuildChunk.generated.h"

class UInstancedStaticMeshComponent;

struct FSurvivalFinishedPiece
{
    FSurvivalBuildPlacement Placement;
    int32 Health;
    int32 MaxHealth;
    int32 StructureId;

    FSurvivalMaterials RepairCost() const
    {
        return Placement.GetPiece().bStone ? FSurvivalMaterials{0, 1} : FSurvivalMaterials{1, 0};
    }
};

UCLASS()
class ASurvivalBuildChunk : public AActor
{
    GENERATED_BODY()

public:
    ASurvivalBuildChunk();

    bool AddPlan(const FSurvivalBuildPlacement& Placement);
    void RemovePlan(int32 Index);
    bool FinishPlan(int32 Index, int32 StructureId);
    bool AddFinishedPiece(const FSurvivalBuildPlacement& Placement, int32 StructureId);
    void RemoveFinishedPieces(const TSet<int32>& StructureIds, int32& DebrisSlots);
    const FSurvivalBuildPlacement* GetPlan(int32 Index) const;
    FSurvivalPlanTarget GetPlanTarget(const FHitResult& Hit);
    const FSurvivalFinishedPiece* FindFinished(const FHitResult& Hit) const;
    bool DamageFinished(const FHitResult& Hit, int32 Amount);
    bool RepairFinished(const FHitResult& Hit, FSurvivalMaterials& Inventory);

protected:
    virtual void BeginPlay() override;

private:
    bool AddInstances(UInstancedStaticMeshComponent& Component, const FSurvivalBuildPlacement& Placement,
        TArray<int32>& Indices);
    void RebuildGhosts();
    int32 FindFinishedIndex(const FHitResult& Hit) const;
    bool AddFinishedInstances(int32 Index);
    void SpawnDebris(const FSurvivalBuildPlacement& Placement);

    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> Timber;

    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> Stone;

    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> Ghosts;

    TArray<FSurvivalBuildPlacement> Plans;
    TMap<int32, int32> GhostOwners;
    TArray<FSurvivalFinishedPiece> Finished;
    TMap<int32, int32> TimberOwners;
    TMap<int32, int32> StoneOwners;
};
