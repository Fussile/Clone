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
    static const FSurvivalBuildPiece Pieces[] = {
        {TEXT("Timber wall"), {3, 0}, false, {Box(-50, -12, 0, 50, 12, 220)}},
        {TEXT("Half wall"), {2, 0}, false, {Box(-50, -12, 0, 50, 12, Rise)}},
        {TEXT("Floor"), {2, 0}, false, {Box(-50, -50, 0, 50, 50, 18)}, true},
        {TEXT("Door frame"), {4, 0}, false, {
            Box(-50, -12, 0, -45, 12, 220), Box(45, -12, 0, 50, 12, 220), Box(-45, -12, 200, 45, 12, 220)}},
        {TEXT("Window wall"), {4, 0}, false, {
            Box(-50, -12, 0, 50, 12, 90), Box(-50, -12, 170, 50, 12, 220),
            Box(-50, -12, 90, -32, 12, 170), Box(32, -12, 90, 50, 12, 170)}},
        {TEXT("Stairs"), {5, 0}, false, {
            Box(-50, -50, 0, 50, -25, Rise / 4), Box(-50, -25, 0, 50, 0, Rise / 2),
            Box(-50, 0, 0, 50, 25, Rise * 3 / 4), Box(-50, 25, 0, 50, 50, Rise)}, false, true},
        {TEXT("Stone wall"), {0, 5}, true, {Box(-50, -14, 0, 50, 14, 220)}},
        {TEXT("Stone foundation"), {0, 4}, true, {Box(-50, -50, 0, 50, 50, 25)}},
        {TEXT("Stone pillar"), {0, 3}, true, {Box(-13, -13, 0, 13, 13, 220)}},
    };
    return MakeArrayView(Pieces);
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
    const bool bLateral = GetPiece().bCarriesLateralSupport && Other.GetPiece().bCarriesLateralSupport;
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
            if (bLateral && FMath::Abs(Box.Min.Z - Source.Min.Z) <= Contact
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
