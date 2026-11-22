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
    Floor.Base.Z = 2.0 * FSurvivalBuildPlacement::GridSize;
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

    FSurvivalBuildPlacement Stairs;
    Stairs.PieceIndex = 5;
    const double Cell = FSurvivalBuildPlacement::GridSize;
    TestTrue(TEXT("Stairs fit a full cube"),
        Stairs.WorldBounds().GetExtent().Equals(FVector(Cell / 2, Cell / 2, Cell / 2), 0.001));
    TestTrue(TEXT("A continuing flight rises exactly one cell"),
        Stairs.StairContinuation().Equals(FVector(0, Cell, Cell), 0.001));

    FSurvivalStructure Cantilever;
    FSurvivalBuildPlacement Timber;
    const int32 Budget = Timber.GetPiece().SupportCapacity();
    Cantilever.Add(1, Budget, true, {}, {});
    FSurvivalBuildPlacement Previous = Timber;
    Previous.Base.Z += Cell;
    Cantilever.Add(2, Budget, false, {{1, Previous.SupportCostFrom(Timber)}}, {});
    for (int32 Id = 3; Id <= 7; ++Id)
    {
        FSurvivalBuildPlacement Next = Previous;
        Next.Base.X += Cell;
        Cantilever.Add(Id, Budget, false, {{Id - 1, Next.SupportCostFrom(Previous)}}, {});
        Previous = Next;
    }
    TestEqual(TEXT("Five cubes can hang from a one-block timber wall"), Cantilever.GetBudget(7), 0);
    TestTrue(TEXT("A sixth block needs more support"), Cantilever.GetBudget(7) < 2);
    TestEqual(TEXT("Removing the footing collapses the entire span"), Cantilever.Remove(1).Num(), 6);

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
    constexpr double Cell = FSurvivalBuildPlacement::GridSize;
    FSurvivalBuildPlacement Wall;
    FSurvivalBuildPlacement Neighbor;
    Neighbor.Rotation = FRotator(0, 90, 0).Quaternion();
    TestTrue(TEXT("Rotating a cube leaves the adjacent cell unchanged"),
        Neighbor.SnapTo(Wall, FVector(0, Cell / 2, 100), FVector::RightVector, FVector::RightVector));
    TestTrue(TEXT("Adjacent cubes share a face"),
        Neighbor.Base.Equals(FVector(0, Cell, 0), Tolerance));
    TestEqual(TEXT("Adjacent walls carry support"), Neighbor.SupportCostFrom(Wall), 2);

    FSurvivalBuildPlacement Roof;
    Roof.PieceIndex = 2;
    TestTrue(TEXT("A roof snaps alongside the wall"),
        Roof.SnapTo(Wall, FVector(Cell / 2, 0, 80), FVector::ForwardVector, FVector::ForwardVector));
    const FBox WallBounds = Wall.WorldBounds();
    const FBox RoofBounds = Roof.WorldBounds();
    TestTrue(TEXT("The wall and roof top surfaces meet flush"),
        FMath::Abs(RoofBounds.Max.Z - WallBounds.Max.Z) < Tolerance);
    TestTrue(TEXT("The roof starts on the wall face without overlapping"),
        FMath::Abs(RoofBounds.Min.X - WallBounds.Max.X) < Tolerance);
    TestEqual(TEXT("The wall carries the roof"), Roof.SupportCostFrom(Wall), 1);
    TestEqual(TEXT("The roof cannot suspend the wall"), Wall.SupportCostFrom(Roof), INDEX_NONE);

    FSurvivalBuildPlacement NextRoof = Roof;
    TestTrue(TEXT("A roof extends by one cell along the exposed edge"),
        NextRoof.SnapTo(Roof, FVector(RoofBounds.Max.X, 0, 90),
            FVector::ForwardVector, FVector::ForwardVector));
    TestTrue(TEXT("Adjacent roof panels stay level"),
        NextRoof.Base.Equals(Roof.Base + FVector(Cell, 0, 0), Tolerance));
    TestEqual(TEXT("A roof panel supports its neighbor"), NextRoof.SupportCostFrom(Roof), 2);

    FSurvivalBuildPlacement Door;
    Door.PieceIndex = 3;
    Door.Rotation = FRotator(0, 90, 0).Quaternion();
    Door.AlignFrameEdge(FVector(25, 0, 0));
    FSurvivalBuildPlacement Reversed = Door;
    Reversed.Rotation = FRotator(0, 270, 0).Quaternion();
    Reversed.AlignFrameEdge(FVector(25, 0, 0));
    TestTrue(TEXT("The frame rests on the edge of the cell"),
        Door.MeshOrigin().Equals(FVector(Cell / 2, 0, 0), Tolerance));
    TestTrue(TEXT("A half-turn reverses the door without moving the frame"),
        Reversed.MeshOrigin().Equals(Door.MeshOrigin(), Tolerance) &&
        Door.Rotation.RotateVector(FVector::RightVector).Equals(
            -Reversed.Rotation.RotateVector(FVector::RightVector), Tolerance));
    TestTrue(TEXT("The thin frame reserves two cells after rotating"),
        Door.ReservedBounds().GetExtent().Equals(FVector(Cell / 2, Cell / 2, Cell), Tolerance));
    TestTrue(TEXT("The frame is physically thin"),
        FMath::Abs(Door.WorldBounds().GetExtent().X - 6.0) < Tolerance);
    TestTrue(TEXT("The doorway opening clears the standing survivor"),
        Door.GetPiece().Boxes.Last().Min.Z > 176.0 && Door.GetPiece().Boxes.Last().GetExtent().X > 34.0);
    FSurvivalBuildPlacement Upper = Wall;
    Upper.Base.Z = Cell;
    TestTrue(TEXT("A cube cannot be placed in a reserved upper doorway cell"),
        Door.OverlapsReservedCells(Upper));
    Upper.Base.Y = Cell;
    TestTrue(TEXT("A cube can use the cell beside the doorway"),
        !Door.OverlapsReservedCells(Upper));
    TestEqual(TEXT("The adjacent upper cube touches the tall door jamb"), Upper.SupportCostFrom(Door), 2);
    TestTrue(TEXT("Two blocks reach the top of the two-cell doorway"),
        FMath::Abs(Upper.WorldBounds().Max.Z - 2.0 * Cell) < Tolerance);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurvivalAuthoredWallTest, "Survival.Building.AuthoredWallCell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSurvivalAuthoredWallTest::RunTest(const FString& Parameters)
{
    constexpr double Cell = FSurvivalBuildPlacement::GridSize;
    constexpr double Half = Cell / 2.0;
    FSurvivalBuildPlacement Panel;
    Panel.PieceIndex = 6;
    Panel.AuthoredWallBounds = FBox(FVector(-Half, 19, 0), FVector(Half, 28, Cell));
    TestTrue(TEXT("The visible wall uses a thin collision box at the outside of the cell"),
        Panel.WorldBounds().Min.Equals(FVector(-Half, 19, 0))
        && Panel.WorldBounds().Max.Equals(FVector(Half, 28, Cell)));

    FSurvivalBuildPlacement Floor;
    Floor.PieceIndex = 2;
    TestTrue(TEXT("The thin wall still reserves every part of the cell"), Panel.OverlapsReservedCells(Floor));
    Floor.Base.Z = Cell;
    TestFalse(TEXT("A floor can use the cell stacked above the thin wall"), Panel.OverlapsReservedCells(Floor));
    TestEqual(TEXT("The thin wall supports the stacked floor"), Floor.SupportCostFrom(Panel), 1);

    FSurvivalBuildPlacement Neighbor = Panel;
    Neighbor.Base.X = Cell;
    TestEqual(TEXT("Adjacent slices carry lateral support"), Panel.SupportCostFrom(Neighbor), 2);

    FSurvivalBuildPlacement Replacement;
    TestTrue(TEXT("A block snaps into the gap beside the remaining panel"),
        Replacement.SnapTo(Neighbor, FVector(Half, 23, 50), -FVector::ForwardVector, -FVector::ForwardVector));
    TestTrue(TEXT("The repair shares the missing slice's cell"), Replacement.Base.Equals(Panel.Base));
    TestFalse(TEXT("The repair does not enter the remaining panel's cell"),
        Neighbor.OverlapsReservedCells(Replacement));
    TestEqual(TEXT("The repair can take support from the slice"), Replacement.SupportCostFrom(Neighbor), 2);
    return true;
}
#endif
