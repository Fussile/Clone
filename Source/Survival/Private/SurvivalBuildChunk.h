#pragma once

#include "GameFramework/Actor.h"
#include "SurvivalBuilding.h"
#include "SurvivalVitals.h"
#include "SurvivalBuildChunk.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

USTRUCT()
struct FSurvivalPlayerPieceState
{
    GENERATED_BODY()

    UPROPERTY()
    FVector Base = FVector::ZeroVector;
    UPROPERTY()
    FVector Offset = FVector::ZeroVector;
    UPROPERTY()
    FQuat Rotation = FQuat::Identity;
    UPROPERTY()
    int32 PieceIndex = 0;
    UPROPERTY()
    int32 Id = INDEX_NONE;
    UPROPERTY()
    int32 Tier = 0;
    UPROPERTY()
    int32 Health = 0;
    UPROPERTY()
    int32 DoorHealth = 0;
    UPROPERTY()
    bool bOpen = false;
    UPROPERTY()
    bool bNegative = false;
};

struct FSurvivalBuildTier
{
    const TCHAR* Name;
    ESurvivalItem Material;
    int32 Cost;
    int32 HitPoints;
};

struct FSurvivalFinishedPiece
{
    FSurvivalBuildPlacement Placement;
    int32 Health;
    int32 MaxHealth;
    int32 StructureId;
    TWeakObjectPtr<UStaticMeshComponent> Door{};
    bool bDoorOpen = false;
    bool bSwingNegative = false;
    int32 DoorHealth = 0;

    int32 Tier = INDEX_NONE;
    const FSurvivalBuildTier* NextTier() const;
    FString UpgradePrompt() const;

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

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Props) const override;
    bool Upgrade(const FHitResult& Hit, int32 ExpectedId, int32 ExpectedTier);
    bool AddPlan(const FSurvivalBuildPlacement& Placement);
    void RemovePlan(int32 Index);
    bool FinishPlan(int32 Index, int32 StructureId);
    bool AddFinishedPiece(const FSurvivalBuildPlacement& Placement, int32 StructureId, bool bFrame = false);
    void RemoveFinishedPieces(const TSet<int32>& StructureIds, int32& DebrisSlots);
    const FSurvivalBuildPlacement* GetPlan(int32 Index) const;
    FSurvivalPlanTarget GetPlanTarget(const FHitResult& Hit);
    const FSurvivalFinishedPiece* FindFinished(const FHitResult& Hit) const;
    bool TryInstallDoor(const FHitResult& Hit);
    bool TryToggleDoor(const FHitResult& Hit, const FVector& UserLocation);
    bool HasReservedCellConflict(const FSurvivalBuildPlacement& Placement, bool bIncludePlans) const;
    bool DamageFinished(const FHitResult& Hit, int32 Amount);
    bool RepairFinished(const FHitResult& Hit, FSurvivalMaterials& Inventory, int32 RepairHealth);

protected:
    virtual void BeginPlay() override;

private:
    bool AddInstances(UInstancedStaticMeshComponent& Component, const FSurvivalBuildPlacement& Placement,
        TArray<int32>& Indices, bool bFrame = false);
    UFUNCTION()
    void RebuildGhosts();
    void RebuildSolids();
    void PublishPlayerPieces();
    UPROPERTY(ReplicatedUsing=ReceivePlayerPieces)
    TArray<FSurvivalPlayerPieceState> PlayerPieces;
    UFUNCTION()
    void ReceivePlayerPieces();
    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> Metal;
    TMap<int32, int32> MetalOwners;
    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> Bracing;
    TMap<int32, int32> BraceOwners;
    int32 FindFinishedIndex(const FHitResult& Hit) const;
    bool AddFinishedInstances(int32 Index);
    void CreateDoorLeaf(FSurvivalFinishedPiece& Piece);
    void RemoveDoorLeaf(FSurvivalFinishedPiece& Piece);
    void SpawnDebris(const FSurvivalBuildPlacement& Placement);

    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> Timber;

    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> Stone;

    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> Ghosts;

    UPROPERTY(ReplicatedUsing=RebuildGhosts)
    TArray<FSurvivalBuildPlacement> Plans;
    TMap<int32, int32> GhostOwners;
    TArray<FSurvivalFinishedPiece> Finished;
    TMap<int32, int32> TimberOwners;
    TMap<int32, int32> StoneOwners;
};
