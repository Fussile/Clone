#pragma once

#include "GameFramework/Actor.h"
#include "SurvivalBuilding.h"
#include "SurvivalBuildChunk.generated.h"

class UInstancedStaticMeshComponent;

UCLASS()
class ASurvivalBuildChunk : public AActor
{
    GENERATED_BODY()

public:
    ASurvivalBuildChunk();

    bool AddPlan(const FSurvivalBuildPlacement& Placement);
    void RemovePlan(int32 Index);
    bool FinishPlan(int32 Index);
    const FSurvivalBuildPlacement* GetPlan(int32 Index) const;
    FSurvivalPlanTarget GetPlanTarget(const FHitResult& Hit);

protected:
    virtual void BeginPlay() override;

private:
    bool AddInstances(UInstancedStaticMeshComponent& Component, const FSurvivalBuildPlacement& Placement,
        TArray<int32>& Indices);
    void RebuildGhosts();

    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> Timber;

    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> Stone;

    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> Ghosts;

    TArray<FSurvivalBuildPlacement> Plans;
    TMap<int32, int32> GhostOwners;
};
