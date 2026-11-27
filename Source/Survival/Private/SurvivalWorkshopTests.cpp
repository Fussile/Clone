#include "SurvivalStation.h"
#include "SurvivalBuildChunk.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurvivalWorkshopCatalogTest, "Survival.Workshop.RecipeGates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSurvivalWorkshopCatalogTest::RunTest(const FString& Parameters)
{
    using I = ESurvivalItem;
    TestTrue(TEXT("Printer accepts the optional power supply"), ASurvivalStation::Slots(I::Printer).Contains(I::PowerSupply));
    int32 CleanRoutes = 0;
    for (const auto& Recipe : GetSurvivalRecipes())
    {
        if (Recipe.Module != I::None)
            TestTrue(TEXT("Required module fits the recipe station"), ASurvivalStation::Slots(Recipe.Station).Contains(Recipe.Module));
        if (Recipe.RequiredTraining != ETraining::Count)
            TestTrue(TEXT("Training is available"), GetTrainingInfo(Recipe.RequiredTraining).bAvailable);
        for (const auto& Ingredient : Recipe.Ingredients)
            TestTrue(TEXT("Nonempty, positive ingredient"), Ingredient.Count > 0 && Ingredient.Item != I::None);
        if (Recipe.Station == I::Printer)
        {
            TestTrue(TEXT("Printer outputs depend on filament and work without an expansion"), Recipe.Module == I::None &&
                Recipe.Ingredients.ContainsByPredicate([](const auto& Input){ return Input.Item == I::Filament || Input.Item == I::ToughFilament; }));
        }
        if (Recipe.Output != I::Water) continue;
        ++CleanRoutes;
        TestTrue(TEXT("Purification requires a heat source and a pot"),
            (Recipe.Station == I::ChemSet && Recipe.RequiredTool == I::Pot) ||
            ((Recipe.Station == I::Campfire || Recipe.Station == I::Stove) && Recipe.Module == I::Pot));
    }
    TestEqual(TEXT("Two water inputs at three stations"), CleanRoutes, 6);
    TestTrue(TEXT("Cloth only modestly reduces contamination"),
        SurvivalWaterIllnessChance(I::RawWater) > SurvivalWaterIllnessChance(I::StrainedWater) &&
        SurvivalWaterIllnessChance(I::StrainedWater) >= .6f);
    TestEqual(TEXT("Purified water has zero illness risk"), SurvivalWaterIllnessChance(I::Water), 0.f);
    FSurvivalProgression Learner;
    for (auto Skill : {ETraining::Workbench, ETraining::Steelworking, ETraining::ChemistryStation})
    {
        TestTrue(TEXT("A fresh character can read a specialist manual"), Learner.CanReadGuide(Skill));
        Learner.ReadGuide(Skill);
        TestEqual(TEXT("The manual unlocks the matching recipes"), Learner.Rank(Skill), 1);
    }
    FSurvivalFinishedPiece Piece{};
    TestTrue(TEXT("Authored structures are excluded"), Piece.NextTier() == nullptr);
    Piece.Tier = 0;
    int32 Health = 35;
    for (auto Material : {I::Timber, I::Timber, I::ScrapIron, I::IronIngot, I::Steel})
    {
        const auto* Tier = Piece.NextTier();
        if (!TestNotNull(TEXT("Upgrade exists"), Tier)) return false;
        TestTrue(TEXT("Material and health progress"), Tier->Material == Material && Tier->Cost > 0 && Tier->HitPoints > Health);
        Health = Tier->HitPoints;
        ++Piece.Tier;
    }
    TestTrue(TEXT("Steel is the final tier"), Piece.NextTier() == nullptr);
    return true;
}
#endif
