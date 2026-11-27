#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Math/Box.h"
#include "SurvivalBuilding.generated.h"

class ASurvivalBuildChunk;

USTRUCT()
struct FSurvivalMaterials
{
    GENERATED_BODY()

    FSurvivalMaterials() = default;
    FSurvivalMaterials(int32 InTimber, int32 InStone) : Timber(InTimber), Stone(InStone) {}

    UPROPERTY()
    int32 Timber = 0;

    UPROPERTY()
    int32 Stone = 0;

    bool IsEmpty() const { return Timber == 0 && Stone == 0; }
    void Add(const FSurvivalMaterials& Other);
    void Spend(const FSurvivalMaterials& Cost);
    FSurvivalMaterials MissingFrom(const FSurvivalMaterials& Inventory) const;
    FString ToText() const;
};

struct FSurvivalBuildPiece
{
    const TCHAR* Name;
    FSurvivalMaterials Cost;
    bool bStone;
    TArray<FBox> Boxes;
    bool bCarriesLateralSupport = false;
    bool bStairs = false;
    bool bWall = false;
    int32 HeightCells = 1;
    bool bDoorFrame = false;

    int32 SupportCapacity() const { return bStone ? 15 : 11; }
};

TConstArrayView<FSurvivalBuildPiece> GetSurvivalBuildPieces();
TConstArrayView<int32> GetPlayerBuildPieces();

USTRUCT()
struct FSurvivalBuildPlacement
{
    GENERATED_BODY()
    static constexpr double GridSize = 105.0;
    static constexpr double HalfWallHeight = GridSize / 2;

    UPROPERTY()
    FVector Base = FVector::ZeroVector;
    UPROPERTY()
    FVector FrameOffset = FVector::ZeroVector;
    UPROPERTY()
    FQuat Rotation = FQuat::Identity;
    UPROPERTY()
    int32 PieceIndex = 0;
    bool bClear = false;

    const FSurvivalBuildPiece& GetPiece() const { return GetSurvivalBuildPieces()[PieceIndex]; }
    FVector MeshOrigin() const { return Base + FrameOffset; }
    void AlignFrameEdge(const FVector& Point);
    FVector WorldCenter(const FBox& Box) const { return MeshOrigin() + Rotation.RotateVector(Box.GetCenter()); }
    FBox WorldBounds() const;
    FBox ReservedBounds() const;
    bool OverlapsReservedCells(const FSurvivalBuildPlacement& Other) const;
    static FBox ClosedDoor();
    bool SnapTo(const FSurvivalBuildPlacement& Source, const FVector& Point,
        const FVector& Normal, const FVector& TowardViewer);
    FVector StairContinuation() const;
    bool IsUpperStairEdge(const FVector& Point, const FVector& Normal) const;
    int32 SupportCostFrom(const FSurvivalBuildPlacement& Other) const;
};

struct FSurvivalPlanTarget
{
    ASurvivalBuildChunk* Chunk = nullptr;
    int32 PlanIndex = INDEX_NONE;

    const FSurvivalBuildPlacement* GetPlan() const;
};
