#pragma once

#include "GameFramework/GameModeBase.h"
#include "SurvivalBuilding.h"
#include "SurvivalStructure.h"
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
    bool CanSupport(const FSurvivalBuildPlacement& Placement) const;
    bool AddAuthoredPiece(const FSurvivalBuildPlacement& Placement);
    void RemoveStructure(int32 Id);
    const FSurvivalMaterials& GetPlannedCost() const { return PlannedCost; }

private:
    struct FRegisteredPiece
    {
        TWeakObjectPtr<ASurvivalBuildChunk> Chunk;
        FSurvivalBuildPlacement Placement;
    };

    ASurvivalBuildChunk* FindOrCreateChunk(const FVector& Base);
    bool HasGroundFooting(const FSurvivalBuildPlacement& Placement) const;
    void FindContacts(const FSurvivalBuildPlacement& Placement, TArray<FSurvivalSupportLink>& Supports,
        TArray<FSurvivalSupportLink>& Dependents) const;
    bool HasSupportedContact(const TArray<FSurvivalSupportLink>& Supports) const;
    void RegisterPiece(int32 Id, ASurvivalBuildChunk& Chunk, const FSurvivalBuildPlacement& Placement,
        bool bGrounded, const TArray<FSurvivalSupportLink>& Supports, const TArray<FSurvivalSupportLink>& Dependents);

    UPROPERTY(Transient)
    TMap<FIntPoint, TObjectPtr<ASurvivalBuildChunk>> BuildChunks;

    FSurvivalMaterials PlannedCost;
    FSurvivalStructure Structure;
    TMap<int32, FRegisteredPiece> RegisteredPieces;
    TMap<FIntVector, TArray<int32>> PiecesByCell;
    int32 NextStructureId = 1;
};
