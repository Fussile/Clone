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

    constexpr double Half = FSurvivalBuildPlacement::GridSize / 2;
    constexpr double Height = FSurvivalBuildPlacement::GridSize;
    constexpr double Rise = FSurvivalBuildPlacement::HalfWallHeight;
    static const FSurvivalBuildPiece Pieces[] = {
        {TEXT("Timber block"), {3, 0}, false, {Box(-Half, -Half, 0, Half, Half, Height)}, false, false, true},
        {TEXT("Half block"), {2, 0}, false, {Box(-Half, -Half, 0, Half, Half, Rise)}, false, false, true},
        {TEXT("Floor"), {2, 0}, false, {Box(-Half, -Half, 0, Half, Half, 18)}, true},
        {TEXT("Door frame"), {4, 0}, false, {
            Box(-Half, -Half, 0, -60, Half, Height), Box(60, -Half, 0, Half, Half, Height),
            Box(-60, -Half, 200, 60, Half, Height)}, false, false, true},
        {TEXT("Window block"), {4, 0}, false, {
            Box(-Half, -Half, 0, Half, Half, 90), Box(-Half, -Half, 170, Half, Half, Height),
            Box(-Half, -Half, 90, -70, Half, 170), Box(70, -Half, 90, Half, Half, 170)}, false, false, true},
        {TEXT("Stairs"), {5, 0}, false, {
            Box(-Half, -Half, 0, Half, -Half / 2, Rise / 4), Box(-Half, -Half / 2, 0, Half, 0, Rise / 2),
            Box(-Half, 0, 0, Half, Half / 2, Rise * 3 / 4), Box(-Half, Half / 2, 0, Half, Half, Rise)},
            false, true},
        {TEXT("Stone block"), {0, 5}, true, {Box(-Half, -Half, 0, Half, Half, Height)}, false, false, true},
        {TEXT("Stone foundation"), {0, 4}, true, {Box(-Half, -Half, 0, Half, Half, 25)}},
        {TEXT("Stone pillar"), {0, 3}, true, {Box(-25, -25, 0, 25, 25, Height)}},
    };
    return MakeArrayView(Pieces);
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
    const bool bTop = Normal.Z >= 0.95;
    const bool bSide = FMath::Abs(Normal.Z) <= 0.1;
    Base = Source.Base;
    const bool bFlushRoof = Piece.bCarriesLateralSupport && Support.bWall;
    if ((bFlushRoof && bTop) || (bSide && (bFlushRoof || Support.bCarriesLateralSupport || Piece.bWall)))
    {
        FVector Side = bTop ? Point - Source.Base : Normal;
        if (bTop && Side.SizeSquared2D() < 1.0)
        {
            Side = TowardViewer;
        }
        const int32 Axis = FMath::Abs(Side.X) >= FMath::Abs(Side.Y) ? 0 : 1;
        Base[Axis] += Side[Axis] >= 0.0 ? GridSize : -GridSize;
        if (bFlushRoof || Support.bCarriesLateralSupport)
        {
            const FBox NewBounds = WorldBounds();
            Base.Z = Bounds.Max.Z - (NewBounds.Max.Z - Base.Z);
        }
        return true;
    }
    if (bTop)
    {
        Base.Z = Point.Z;
        return true;
    }
    if (bSide && Piece.bCarriesLateralSupport)
    {
        Base.Z = Bounds.Max.Z;
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
            const bool bEdge = (bX && (FMath::Abs(Box.Min.Y - Source.Max.Y) <= Contact
                    || FMath::Abs(Box.Max.Y - Source.Min.Y) <= Contact))
                || (bY && (FMath::Abs(Box.Min.X - Source.Max.X) <= Contact
                    || FMath::Abs(Box.Max.X - Source.Min.X) <= Contact));
            if (bZ && bEdge)
            {
                if (GetPiece().bCarriesLateralSupport && Other.GetPiece().bWall
                    && FMath::Abs(Box.Max.Z - Source.Max.Z) <= Contact)
                {
                    return 1;
                }
                if (bLateral && FMath::Abs(Base.Z - Other.Base.Z) <= Contact)
                {
                    BestCost = 2;
                }
            }
        }
    }
    return BestCost;
}
