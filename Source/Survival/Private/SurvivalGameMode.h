#pragma once

#include "GameFramework/GameModeBase.h"
#include "SurvivalBuilding.h"
#include "SurvivalGameMode.generated.h"

class ASurvivalBuildChunk;

UCLASS()
class ASurvivalGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ASurvivalGameMode();

    bool AddPlan(const FSurvivalBuildPlacement& Placement);
    void RemovePlan(const FSurvivalPlanTarget& Target);
    bool FinishPlan(const FSurvivalPlanTarget& Target, FSurvivalMaterials& Inventory);
    const FSurvivalMaterials& GetPlannedCost() const { return PlannedCost; }

private:
    UPROPERTY(Transient)
    TMap<FIntPoint, TObjectPtr<ASurvivalBuildChunk>> BuildChunks;

    FSurvivalMaterials PlannedCost;
};
