#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Math/Box.h"

class ASurvivalBuildChunk;

struct FSurvivalMaterials
{
    int32 Timber = 0;
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

    int32 SupportCapacity() const { return bStone ? 9 : 5; }
};

TConstArrayView<FSurvivalBuildPiece> GetSurvivalBuildPieces();

struct FSurvivalBuildPlacement
{
    static constexpr double GridSize = 100.0;
    static constexpr double PlacementGrid = 25.0;
    static constexpr double HalfWallHeight = 110.0;

    FVector Base = FVector::ZeroVector;
    FQuat Rotation = FQuat::Identity;
    int32 PieceIndex = 0;
    bool bClear = false;

    const FSurvivalBuildPiece& GetPiece() const { return GetSurvivalBuildPieces()[PieceIndex]; }
    FVector WorldCenter(const FBox& Box) const { return Base + Rotation.RotateVector(Box.GetCenter()); }
    FBox WorldBounds() const;
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
