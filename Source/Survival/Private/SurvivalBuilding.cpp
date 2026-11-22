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
    constexpr double Step = Height / 5;
    constexpr double DoorHeight = Height * 2;
    static const TArray<FBox> Frame = {Box(-Half, -6, 0, -Half + 8, 6, DoorHeight),
        Box(Half - 8, -6, 0, Half, 6, DoorHeight), Box(-Half + 8, -6, DoorHeight - 8, Half - 8, 6, DoorHeight)};
    static const FSurvivalBuildPiece Pieces[] = {
        {TEXT("Timber block"), {3, 0}, false, {Box(-Half, -Half, 0, Half, Half, Height)}, false, false, true},
        {TEXT("Half block"), {2, 0}, false, {Box(-Half, -Half, 0, Half, Half, Rise)}, false, false, true},
        {TEXT("Floor"), {2, 0}, false, {Box(-Half, -Half, 0, Half, Half, 18)}, true},
        {TEXT("Door frame"), {4, 0}, false, Frame, false, false, true, 2, true},
        {TEXT("Window block"), {3, 0}, false, {
            Box(-Half, -Half, 0, Half, Half, 25), Box(-Half, -Half, 85, Half, Half, Height),
            Box(-Half, -Half, 25, -35, Half, 85), Box(35, -Half, 25, Half, Half, 85)}, false, false, true},
        {TEXT("Stairs"), {5, 0}, false, {
            Box(-Half, -Half, 0, Half, -Half + Step, Step),
            Box(-Half, -Half + Step, 0, Half, -Half + Step * 2, Step * 2),
            Box(-Half, -Half + Step * 2, 0, Half, -Half + Step * 3, Step * 3),
            Box(-Half, -Half + Step * 3, 0, Half, -Half + Step * 4, Step * 4),
            Box(-Half, -Half + Step * 4, 0, Half, Half, Height)},
            false, true},
        {TEXT("Stone block"), {0, 5}, true, {Box(-Half, -Half, 0, Half, Half, Height)}, false, false, true},
        {TEXT("Stone foundation"), {0, 4}, true, {Box(-Half, -Half, 0, Half, Half, 25)}},
        {TEXT("Stone pillar"), {0, 3}, true, {Box(-25, -25, 0, 25, 25, DoorHeight)}, false, false, false, 2},
    };
    return MakeArrayView(Pieces);
}

TConstArrayView<int32> GetPlayerBuildPieces()
{
    // The other stable indices are reserved for authored prefabs.
    static const int32 Pieces[] = {0, 6, 5, 3};
    return MakeArrayView(Pieces);
}

void FSurvivalBuildPlacement::AlignFrameEdge(const FVector& Point)
{
    if (!GetPiece().bDoorFrame)
    {
        return;
    }
    const FVector Normal = Rotation.RotateVector(FVector::RightVector);
    const int32 Axis = FMath::Abs(Normal.X) > FMath::Abs(Normal.Y) ? 0 : 1;
    FrameOffset = FVector::ZeroVector;
    FrameOffset[Axis] = (Point[Axis] >= Base[Axis] ? 1.0 : -1.0) * GridSize / 2.0;
}

TConstArrayView<FBox> FSurvivalBuildPlacement::CollisionBoxes() const
{
    if (AuthoredWallBounds.IsValid)
    {
        return TConstArrayView<FBox>(&AuthoredWallBounds, 1);
    }
    return GetPiece().Boxes;
}

FBox FSurvivalBuildPlacement::WorldBounds() const
{
    FBox Bounds(ForceInit);
    const FTransform Transform(Rotation, MeshOrigin());
    for (const FBox& Box : CollisionBoxes())
    {
        Bounds += Box.TransformBy(Transform);
    }
    return Bounds;
}

FBox FSurvivalBuildPlacement::ReservedBounds() const
{
    if (!ReservesFullCells())
    {
        return WorldBounds();
    }
    constexpr double Half = GridSize / 2;
    return FBox(Base - FVector(Half, Half, 0), Base + FVector(Half, Half, GridSize * GetPiece().HeightCells));
}

bool FSurvivalBuildPlacement::OverlapsReservedCells(const FSurvivalBuildPlacement& Other) const
{
    if (!ReservesFullCells() && !Other.ReservesFullCells())
    {
        return false;
    }
    const FBox A = ReservedBounds();
    const FBox B = Other.ReservedBounds();
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        if (FMath::Min(A.Max[Axis], B.Max[Axis]) - FMath::Max(A.Min[Axis], B.Min[Axis]) <= 0.5)
        {
            return false;
        }
    }
    return true;
}

FBox FSurvivalBuildPlacement::ClosedDoor()
{
    return FBox(FVector(-41.5, -2.5, 2), FVector(41.5, 2.5, 200));
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
        else if (Support.HeightCells > 1 && Piece.HeightCells == 1)
        {
            const double Layer = FMath::FloorToInt((Point.Z - Source.Base.Z) / GridSize);
            Base.Z += FMath::Clamp(Layer, 0.0, static_cast<double>(Support.HeightCells - 1)) * GridSize;
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
    return Base + Rotation.RotateVector(FVector(0, GridSize, GridSize));
}

bool FSurvivalBuildPlacement::IsUpperStairEdge(const FVector& Point, const FVector& Normal) const
{
    if (!GetPiece().bStairs)
    {
        return false;
    }
    const FVector LocalPoint = Rotation.Inverse().RotateVector(Point - Base);
    const FVector LocalNormal = Rotation.Inverse().RotateVector(Normal);
    return (LocalNormal.Z >= 0.95 && LocalPoint.Z >= GridSize - 1.0)
        || (LocalNormal.Y >= 0.95 && LocalPoint.Z >= GridSize * 0.75);
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
    const FTransform ThisTransform(Rotation, MeshOrigin());
    const FTransform SourceTransform(Other.Rotation, Other.MeshOrigin());
    const bool bLateral = (GetPiece().bCarriesLateralSupport && Other.GetPiece().bCarriesLateralSupport)
        || (GetPiece().bWall && Other.GetPiece().bWall);
    int32 BestCost = INDEX_NONE;
    for (const FBox& LocalBox : CollisionBoxes())
    {
        const FBox Box = LocalBox.TransformBy(ThisTransform);
        for (const FBox& SourceLocal : Other.CollisionBoxes())
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
                const double HeightDifference = Base.Z - Other.Base.Z;
                const bool bSameLayer = FMath::Abs(HeightDifference) <= Contact;
                const bool bTallWallJoint = GetPiece().bWall && Other.GetPiece().bWall
                    && (GetPiece().HeightCells > 1 || Other.GetPiece().HeightCells > 1)
                    && FMath::Abs(HeightDifference - FMath::GridSnap(HeightDifference, GridSize)) <= Contact;
                if (bLateral && (bSameLayer || bTallWallJoint))
                {
                    BestCost = 2;
                }
            }
        }
    }
    return BestCost;
}
