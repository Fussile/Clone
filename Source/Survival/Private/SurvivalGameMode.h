#pragma once

#include "GameFramework/GameModeBase.h"
#include "SurvivalBuilding.h"
#include "SurvivalStructure.h"
#include "SurvivalSupplyCrate.h"
#include "SurvivalGameMode.generated.h"

class ADirectionalLight;
class ASurvivalBuildChunk;

UCLASS(Config = Game)
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
    bool BeginAuthoredPrefab(FName PrefabId);
    bool AddAuthoredPiece(const FSurvivalBuildPlacement& Placement);
    void RemoveStructure(int32 Id);
    const FSurvivalMaterials& GetPlannedCost() const { return PlannedCost; }
    int32 GetWorldMinutes() const;
    double GetAmbientTemperature(const FVector& Position) const;
    bool SetTimeOfDay(double Hour);
    void RegisterScavengeObject(ASurvivalSupplyCrate& Object);
    void UnregisterScavengeObject(ASurvivalSupplyCrate& Object);
    void RememberScavengeObject(FName Id, const FSurvivalScavengeState& State);

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
    void UpdateScavengeCycle();
    void RefreshScavengeObject(ASurvivalSupplyCrate& Object) const;

    UPROPERTY(EditDefaultsOnly, Config, Category = "Scavenging", meta = (ClampMin = "1"))
    int32 LootRefreshDays = 3;

    TSet<TWeakObjectPtr<ASurvivalSupplyCrate>> ScavengeObjects;
    TMap<FName, FSurvivalScavengeState> ScavengeState;
    int32 LootCycle = 0;
    int32 LootSeed = 0;

    TOptional<FVector> ClimateBoundaries;

    UPROPERTY(EditDefaultsOnly, Category = "Survival", meta = (ClampMin = "60"))
    float DayLengthSeconds = 3600.0f;

    TWeakObjectPtr<ADirectionalLight> Sun;
    TWeakObjectPtr<ADirectionalLight> Moon;
    float DaylightIntensity = 0.0f;
    float MoonlightIntensity = 0.0f;
    double NextHordeAttempt = 0.0;
    bool bFirstHordeStarted = false;
    double ElapsedSurvivalSeconds = 0.0;
    FSurvivalMaterials PlannedCost;
    FSurvivalStructure Structure;
    TMap<int32, FRegisteredPiece> RegisteredPieces;
    TMap<FIntVector, TArray<int32>> PiecesByCell;
    TSet<FName> InitializedPrefabs;
    int32 NextStructureId = 1;
};
