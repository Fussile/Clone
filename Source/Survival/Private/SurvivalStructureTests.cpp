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
#endif
