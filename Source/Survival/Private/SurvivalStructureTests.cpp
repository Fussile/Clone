#include "SurvivalStructure.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SurvivalBuilding.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurvivalCollapseTest, "Survival.Building.StructuralCollapse",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSurvivalCollapseTest::RunTest(const FString& Parameters)
{
    FSurvivalStructure Graph;
    Graph.Add(1, 9, true, {}, {});
    Graph.Add(2, 5, false, {{1, 1}}, {});
    Graph.Add(3, 5, false, {{2, 2}}, {{2, 2}});
    Graph.Add(4, 9, true, {}, {{3, 1}});
    Graph.Add(5, 5, false, {{3, 2}}, {{3, 2}});
    TestEqual(TEXT("Second footing restores the bridged floor"), Graph.GetBudget(3), 5);
    TestEqual(TEXT("Lateral transfer has a higher cost"), Graph.GetBudget(5), 3);

    TestTrue(TEXT("A bridge with an alternate footing holds"), Graph.Remove(1).IsEmpty());
    TestEqual(TEXT("The bridge is now supported from the other side"), Graph.GetBudget(2), 3);
    const TArray<int32> Collapsed = Graph.Remove(4);
    TestEqual(TEXT("A disconnected cycle loses all of its pieces"), Collapsed.Num(), 3);
    TestEqual(TEXT("A fallen piece is removed from the graph"), Graph.GetBudget(3), INDEX_NONE);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurvivalContactsTest, "Survival.Building.StructuralContacts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSurvivalContactsTest::RunTest(const FString& Parameters)
{
    FSurvivalBuildPlacement Pillar;
    Pillar.PieceIndex = 8;
    FSurvivalBuildPlacement Floor;
    Floor.PieceIndex = 2;
    Floor.Base.Z = 220;
    TestEqual(TEXT("Pillar top supports the roof"), Floor.SupportCostFrom(Pillar), 1);
    TestEqual(TEXT("Roof cannot support a pillar below it"), Pillar.SupportCostFrom(Floor), INDEX_NONE);

    FSurvivalBuildPlacement Adjacent = Floor;
    Adjacent.Base.X += FSurvivalBuildPlacement::GridSize;
    TestEqual(TEXT("Floors carry edge support"), Adjacent.SupportCostFrom(Floor), 2);
    Adjacent.Base.Y += FSurvivalBuildPlacement::GridSize;
    TestEqual(TEXT("Corner contact carries no support"), Adjacent.SupportCostFrom(Floor), INDEX_NONE);
    Adjacent.Base.X = 0;
    Adjacent.Base.Y = 0;
    Adjacent.Base.Z += 50;
    TestEqual(TEXT("An air gap carries no support"), Adjacent.SupportCostFrom(Floor), INDEX_NONE);

    FSurvivalBuildPlacement HalfWall;
    HalfWall.PieceIndex = 1;
    FSurvivalBuildPlacement Stairs;
    Stairs.PieceIndex = 5;
    TestEqual(TEXT("Stairs rise to the half-wall height"), Stairs.GetPiece().Boxes.Last().Max.Z,
        HalfWall.GetPiece().Boxes.Last().Max.Z);
    FSurvivalBuildPlacement NextStairs = Stairs;
    NextStairs.Base = Stairs.StairContinuation();
    TestEqual(TEXT("A flush stair flight supports the flight above it"), NextStairs.SupportCostFrom(Stairs), 1);
    TestEqual(TEXT("The higher flight cannot support the lower one"), Stairs.SupportCostFrom(NextStairs), INDEX_NONE);
    NextStairs.Base.Z += 2;
    TestEqual(TEXT("A vertical gap breaks the stair joint"), NextStairs.SupportCostFrom(Stairs), INDEX_NONE);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurvivalEdgeSnapTest, "Survival.Building.EdgeSnap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSurvivalEdgeSnapTest::RunTest(const FString& Parameters)
{
    constexpr double Tolerance = 0.001;
    FSurvivalBuildPlacement Wall;
    FSurvivalBuildPlacement Corner;
    Corner.Rotation = FRotator(0, 90, 0).Quaternion();
    TestTrue(TEXT("A rotated wall snaps to the end of its neighbor"),
        Corner.SnapTo(Wall, FVector(48, 12.5, 100), FVector::RightVector, FVector::RightVector));
    TestTrue(TEXT("Corner walls touch without sharing any interior volume"),
        Corner.Base.Equals(FVector(37.5, 62.5, 0), Tolerance));
    TestEqual(TEXT("The butt joint is load-bearing"), Corner.SupportCostFrom(Wall), 2);

    FSurvivalBuildPlacement Back;
    Back.Rotation = FRotator(0, 180, 0).Quaternion();
    TestTrue(TEXT("A second corner snaps in the other axis"),
        Back.SnapTo(Corner, FVector(25, 112, 100), -FVector::ForwardVector, -FVector::ForwardVector));
    FSurvivalBuildPlacement Left;
    Left.Rotation = FRotator(0, 270, 0).Quaternion();
    TestTrue(TEXT("A fourth wall closes the room"),
        Left.SnapTo(Back, FVector(-60, 87.5, 100), -FVector::RightVector, -FVector::RightVector));
    TestEqual(TEXT("The last corner meets the first wall"), Wall.SupportCostFrom(Left), 2);

    FSurvivalBuildPlacement Roof;
    Roof.PieceIndex = 2;
    TestTrue(TEXT("A roof snaps at the top of a wall face"),
        Roof.SnapTo(Wall, FVector(0, 12.5, 150), FVector::RightVector, FVector::RightVector));
    TestTrue(TEXT("Roof has a flush exterior edge and sits on the wall"),
        Roof.Base.Equals(FVector(0, 37.5, 220), Tolerance));
    TestEqual(TEXT("The wall carries the roof"), Roof.SupportCostFrom(Wall), 1);

    FSurvivalBuildPlacement Strip;
    Strip.PieceIndex = 10;
    Strip.Rotation = FRotator(0, 90, 0).Quaternion();
    const FBox RoofBounds = Roof.WorldBounds();
    TestTrue(TEXT("A 25 cm strip snaps to a full floor edge"),
        Strip.SnapTo(Roof, FVector(0, RoofBounds.Max.Y, 229), FVector::RightVector, FVector::RightVector));
    TestTrue(TEXT("The strip meets the floor edge without overlapping it"),
        FMath::Abs(Strip.WorldBounds().Min.Y - RoofBounds.Max.Y) < Tolerance);
    TestEqual(TEXT("The strip receives lateral support"), Strip.SupportCostFrom(Roof), 2);

    FSurvivalBuildPlacement Infill;
    Infill.PieceIndex = 11;
    TestTrue(TEXT("The corner infill snaps to the end of a narrow strip"),
        Infill.SnapTo(Strip, FVector(-50, 100, 229), -FVector::ForwardVector, -FVector::ForwardVector));
    TestTrue(TEXT("The infill occupies the missing 25 cm square"),
        Infill.Base.Equals(FVector(-62.5, 100, 220), Tolerance));
    TestEqual(TEXT("The infill receives support from the strip"), Infill.SupportCostFrom(Strip), 2);

    FSurvivalBuildPlacement Fill;
    Fill.PieceIndex = 9;
    TestTrue(TEXT("A quarter wall aligns with the end of a full wall"),
        Fill.SnapTo(Wall, FVector(50, 0, 100), FVector::ForwardVector, FVector::ForwardVector));
    TestTrue(TEXT("The quarter wall spans one grid interval"),
        Fill.Base.Equals(FVector(62.5, 0, 0), Tolerance));
    return true;
}
#endif
