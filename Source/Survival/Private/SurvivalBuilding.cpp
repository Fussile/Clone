#include "SurvivalBuilding.h"

void FSurvivalMaterials::Add(const FSurvivalMaterials& Other)
{
    Timber += Other.Timber;
    Stone += Other.Stone;
}

void FSurvivalMaterials::Spend(const FSurvivalMaterials& Cost)
{
    Timber -= Cost.Timber;
    Stone -= Cost.Stone;
}

FSurvivalMaterials FSurvivalMaterials::MissingFrom(const FSurvivalMaterials& Inventory) const
{
    return {FMath::Max(0, Timber - Inventory.Timber), FMath::Max(0, Stone - Inventory.Stone)};
}

FString FSurvivalMaterials::ToText() const
{
    if (Timber == 0)
    {
        return Stone == 0 ? TEXT("None") : FString::Printf(TEXT("%d stone"), Stone);
    }
    if (Stone == 0)
    {
        return FString::Printf(TEXT("%d timber"), Timber);
    }
    return FString::Printf(TEXT("%d timber / %d stone"), Timber, Stone);
}

TConstArrayView<FSurvivalBuildPiece> GetSurvivalBuildPieces()
{
    const auto Box = [](double X0, double Y0, double Z0, double X1, double Y1, double Z1)
    {
        return FBox(FVector(X0, Y0, Z0), FVector(X1, Y1, Z1));
    };

    constexpr double Rise = FSurvivalBuildPlacement::HalfWallHeight;
    constexpr double T = FSurvivalBuildPlacement::PlacementGrid / 2;
    static const FSurvivalBuildPiece Pieces[] = {
        {TEXT("Timber wall"), {3, 0}, false, {Box(-50, -T, 0, 50, T, 220)}, false, false, true},
        {TEXT("Half wall"), {2, 0}, false, {Box(-50, -T, 0, 50, T, Rise)}, false, false, true},
        {TEXT("Floor"), {2, 0}, false, {Box(-50, -50, 0, 50, 50, 18)}, true},
        {TEXT("Door frame"), {4, 0}, false, {
            Box(-50, -T, 0, -45, T, 220), Box(45, -T, 0, 50, T, 220), Box(-45, -T, 200, 45, T, 220)},
            false, false, true},
        {TEXT("Window wall"), {4, 0}, false, {
            Box(-50, -T, 0, 50, T, 90), Box(-50, -T, 170, 50, T, 220),
            Box(-50, -T, 90, -32, T, 170), Box(32, -T, 90, 50, T, 170)}, false, false, true},
        {TEXT("Stairs"), {5, 0}, false, {
            Box(-50, -50, 0, 50, -25, Rise / 4), Box(-50, -25, 0, 50, 0, Rise / 2),
            Box(-50, 0, 0, 50, 25, Rise * 3 / 4), Box(-50, 25, 0, 50, 50, Rise)}, false, true},
        {TEXT("Stone wall"), {0, 5}, true, {Box(-50, -T, 0, 50, T, 220)}, false, false, true},
        {TEXT("Stone foundation"), {0, 4}, true, {Box(-50, -50, 0, 50, 50, 25)}},
        {TEXT("Stone pillar"), {0, 3}, true, {Box(-13, -13, 0, 13, 13, 220)}},
        {TEXT("Quarter wall"), {1, 0}, false, {Box(-T, -T, 0, T, T, 220)}, false, false, true},
        {TEXT("Quarter floor strip"), {1, 0}, false, {Box(-T, -50, 0, T, 50, 18)}, true},
        {TEXT("Floor corner infill"), {1, 0}, false, {Box(-T, -T, 0, T, T, 18)}, true},
    };
    return MakeArrayView(Pieces);
}

namespace
{
int32 HorizontalAxis(const FVector& Direction)
{
    return FMath::Abs(Direction.X) >= FMath::Abs(Direction.Y) ? 0 : 1;
}

double SnapAlongEdge(const FBox& Source, const FVector& Extent, int32 Axis, double Aim)
{
    const double First = Source.Min[Axis] + Extent[Axis];
    const double Last = Source.Max[Axis] - Extent[Axis];
    if (First > Last)
    {
        // Larger panels can align their near or far end with a smaller support.
        return Aim < Source.GetCenter()[Axis] ? Last : First;
    }
    return FMath::Clamp(First + FMath::GridSnap(Aim - First, FSurvivalBuildPlacement::PlacementGrid), First, Last);
}
}

FBox FSurvivalBuildPlacement::WorldBounds() const
{
    FBox Bounds(ForceInit);
    const FTransform Transform(Rotation, Base);
    for (const FBox& Box : GetPiece().Boxes)
    {
        Bounds += Box.TransformBy(Transform);
    }
    return Bounds;
}

bool FSurvivalBuildPlacement::SnapTo(const FSurvivalBuildPlacement& Source, const FVector& Point,
    const FVector& Normal, const FVector& TowardViewer)
{
    const FSurvivalBuildPiece& Piece = GetPiece();
    const FSurvivalBuildPiece& Support = Source.GetPiece();
    if (Piece.bStairs && Source.IsUpperStairEdge(Point, Normal))
    {
        Base = Source.StairContinuation();
        Rotation = Source.Rotation;
        return true;
    }

    const FBox Bounds = Source.WorldBounds();
    const FVector Extent = WorldBounds().GetExtent();
    const FVector Center = Bounds.GetCenter();
    const bool bTop = Normal.Z >= 0.95;
    const bool bSide = FMath::Abs(Normal.Z) <= 0.1;
    const int32 Run = HorizontalAxis(Source.Rotation.RotateVector(FVector::ForwardVector));
    const int32 Face = 1 - Run;
    const int32 NewRun = HorizontalAxis(Rotation.RotateVector(FVector::ForwardVector));

    if (Piece.bCarriesLateralSupport && Support.bWall && (bTop || bSide))
    {
        // The panel overlaps the top bearing surface and ends flush with the far wall face.
        const double Side = (bSide && FMath::Abs(Normal[Face]) > 0.95 ? Normal[Face] : TowardViewer[Face]);
        Base[Face] = Side >= 0.0 ? Bounds.Min[Face] + Extent[Face] : Bounds.Max[Face] - Extent[Face];
        Base[Run] = SnapAlongEdge(Bounds, Extent, Run, Point[Run]);
        Base.Z = Bounds.Max.Z;
        return true;
    }
    if (Piece.bCarriesLateralSupport && Support.bCarriesLateralSupport && bSide)
    {
        const int32 Edge = HorizontalAxis(Normal);
        Base[Edge] = Normal[Edge] >= 0.0 ? Bounds.Max[Edge] + Extent[Edge] : Bounds.Min[Edge] - Extent[Edge];
        Base[1 - Edge] = SnapAlongEdge(Bounds, Extent, 1 - Edge, Point[1 - Edge]);
        Base.Z = Source.Base.Z;
        return true;
    }
    if (Piece.bWall && Support.bWall && bSide)
    {
        const bool bFarEnd = Point[Run] >= Center[Run];
        if (NewRun == Run)
        {
            Base[Run] = bFarEnd ? Bounds.Max[Run] + Extent[Run] : Bounds.Min[Run] - Extent[Run];
            Base[Face] = Center[Face];
        }
        else
        {
            const bool bFarFace = (FMath::Abs(Normal[Face]) > 0.95
                ? Normal[Face] : TowardViewer[Face]) >= 0.0;
            Base[Run] = bFarEnd ? Bounds.Max[Run] - Extent[Run] : Bounds.Min[Run] + Extent[Run];
            Base[Face] = bFarFace ? Bounds.Max[Face] + Extent[Face] : Bounds.Min[Face] - Extent[Face];
        }
        Base.Z = Source.Base.Z;
        return true;
    }
    if (bTop)
    {
        if (Piece.bWall && Support.bCarriesLateralSupport)
        {
            const int32 Edge = 1 - NewRun;
            Base[Edge] = Point[Edge] >= Center[Edge] ? Bounds.Max[Edge] - Extent[Edge] : Bounds.Min[Edge] + Extent[Edge];
            Base[NewRun] = SnapAlongEdge(Bounds, Extent, NewRun, Point[NewRun]);
        }
        else if (Support.bWall || Support.bCarriesLateralSupport || Support.bStairs)
        {
            Base.X = SnapAlongEdge(Bounds, Extent, 0, Point.X);
            Base.Y = SnapAlongEdge(Bounds, Extent, 1, Point.Y);
        }
        else
        {
            Base.X = Source.Base.X;
            Base.Y = Source.Base.Y;
        }
        Base.Z = Point.Z;
        return true;
    }
    if (Piece.bCarriesLateralSupport && bSide)
    {
        Base = FVector(Source.Base.X, Source.Base.Y, Bounds.Max.Z);
        return true;
    }
    return false;
}

FVector FSurvivalBuildPlacement::StairContinuation() const
{
    return Base + Rotation.RotateVector(FVector(0, GridSize, HalfWallHeight));
}

bool FSurvivalBuildPlacement::IsUpperStairEdge(const FVector& Point, const FVector& Normal) const
{
    if (!GetPiece().bStairs)
    {
        return false;
    }
    const FVector LocalPoint = Rotation.Inverse().RotateVector(Point - Base);
    const FVector LocalNormal = Rotation.Inverse().RotateVector(Normal);
    return (LocalNormal.Z >= 0.95 && LocalPoint.Z >= HalfWallHeight - 1.0)
        || (LocalNormal.Y >= 0.95 && LocalPoint.Z >= HalfWallHeight * 0.75);
}

int32 FSurvivalBuildPlacement::SupportCostFrom(const FSurvivalBuildPlacement& Other) const
{
    constexpr double Contact = 1.0;
    if (GetPiece().bStairs && Other.GetPiece().bStairs
        && Rotation.Equals(Other.Rotation) && Base.Equals(Other.StairContinuation(), Contact))
    {
        return 1;
    }
    const auto Overlaps = [](double MinA, double MaxA, double MinB, double MaxB)
    {
        return FMath::Min(MaxA, MaxB) - FMath::Max(MinA, MinB) > 1.0;
    };
    const FTransform ThisTransform(Rotation, Base);
    const FTransform SourceTransform(Other.Rotation, Other.Base);
    const bool bLateral = (GetPiece().bCarriesLateralSupport && Other.GetPiece().bCarriesLateralSupport)
        || (GetPiece().bWall && Other.GetPiece().bWall);
    int32 BestCost = INDEX_NONE;
    for (const FBox& LocalBox : GetPiece().Boxes)
    {
        const FBox Box = LocalBox.TransformBy(ThisTransform);
        for (const FBox& SourceLocal : Other.GetPiece().Boxes)
        {
            const FBox Source = SourceLocal.TransformBy(SourceTransform);
            const bool bX = Overlaps(Box.Min.X, Box.Max.X, Source.Min.X, Source.Max.X);
            const bool bY = Overlaps(Box.Min.Y, Box.Max.Y, Source.Min.Y, Source.Max.Y);
            if (bX && bY && FMath::Abs(Box.Min.Z - Source.Max.Z) <= Contact)
            {
                return 1;
            }
            const bool bZ = Overlaps(Box.Min.Z, Box.Max.Z, Source.Min.Z, Source.Max.Z);
            if (bLateral && bZ && FMath::Abs(Base.Z - Other.Base.Z) <= Contact
                && ((bX && (FMath::Abs(Box.Min.Y - Source.Max.Y) <= Contact
                        || FMath::Abs(Box.Max.Y - Source.Min.Y) <= Contact))
                    || (bY && (FMath::Abs(Box.Min.X - Source.Max.X) <= Contact
                        || FMath::Abs(Box.Max.X - Source.Min.X) <= Contact))))
            {
                BestCost = 2;
            }
        }
    }
    return BestCost;
}
