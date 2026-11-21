#pragma once

#include "GameFramework/GameModeBase.h"
#include "SurvivalBuilding.h"
#include "SurvivalStructure.h"
#include "SurvivalGameMode.generated.h"

class ADirectionalLight;
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
    bool HasReservedCellConflict(const FSurvivalBuildPlacement& Placement, bool bIncludePlans) const;
    bool CanSupport(const FSurvivalBuildPlacement& Placement) const;
    bool AddAuthoredPiece(const FSurvivalBuildPlacement& Placement);
    void RemoveStructure(int32 Id);
    const FSurvivalMaterials& GetPlannedCost() const { return PlannedCost; }
    int32 GetWorldMinutes() const;
    double GetAmbientTemperature() const;
    bool SetTimeOfDay(double Hour);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

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

    void UpdateDaylight();

    UPROPERTY(EditDefaultsOnly, Category = "Survival", meta = (ClampMin = "60"))
    float DayLengthSeconds = 1200.0f;

    TWeakObjectPtr<ADirectionalLight> Sun;
    TWeakObjectPtr<ADirectionalLight> Moon;
    float DaylightIntensity = 0.0f;
    float MoonlightIntensity = 0.0f;
    double ElapsedSurvivalSeconds = 0.0;
    FSurvivalMaterials PlannedCost;
    FSurvivalStructure Structure;
    TMap<int32, FRegisteredPiece> RegisteredPieces;
    TMap<FIntVector, TArray<int32>> PiecesByCell;
    int32 NextStructureId = 1;
};
