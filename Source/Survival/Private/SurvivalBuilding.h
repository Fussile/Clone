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
};

TConstArrayView<FSurvivalBuildPiece> GetSurvivalBuildPieces();

struct FSurvivalBuildPlacement
{
    static constexpr double GridSize = 100.0;

    FVector Base = FVector::ZeroVector;
    FQuat Rotation = FQuat::Identity;
    int32 PieceIndex = 0;
    bool bClear = false;

    const FSurvivalBuildPiece& GetPiece() const { return GetSurvivalBuildPieces()[PieceIndex]; }
    FVector WorldCenter(const FBox& Box) const { return Base + Rotation.RotateVector(Box.GetCenter()); }
};

struct FSurvivalPlanTarget
{
    ASurvivalBuildChunk* Chunk = nullptr;
    int32 PlanIndex = INDEX_NONE;

    const FSurvivalBuildPlacement* GetPlan() const;
};
