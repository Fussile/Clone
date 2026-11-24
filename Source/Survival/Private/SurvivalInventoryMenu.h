#pragma once

#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"
#include "SurvivalVitals.h"
#include "Widgets/SCompoundWidget.h"

class ASurvivalCharacter;
class ASurvivalHUD;
class SSearchBox;
class SScrollBox;

class SSurvivalInventoryMenu : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SSurvivalInventoryMenu) {}
        SLATE_ARGUMENT(ASurvivalCharacter*, Player)
        SLATE_ARGUMENT(ASurvivalHUD*, HUD)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);
    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
    const FSlateBrush* FindThemeIcon(const TCHAR* Name) const;
    const FSlateBrush* FindIcon(ESurvivalItem Item) const;
    bool MatchesFilters(const FSurvivalRecipe& Recipe) const;
    void SelectFirstMatchingRecipe();
    TSharedRef<SWidget> MakeRecipeBrowser();
    TSharedRef<SWidget> MakeSkillsPanel();
    TSharedRef<SWidget> MakeStatsPanel();
    TSharedRef<SWidget> MakeLootPanel();
    TArray<FSlateBrush> GearIcons;
    ETraining SelectedStat = ETraining::Athletics;
    TSharedRef<SWidget> MakeTrainingDetails();
    bool MatchesTraining(ETraining Entry) const;
    void SelectFirstTraining();
    void SelectTraining(int32 Index);
    FText TrainingRequirements() const;
    TSharedRef<SWidget> MakeRecipeDetails();
    FText GetIngredientText() const;

    TSharedPtr<SSearchBox> RecipeFilterBox;
    FString SearchText;
    FString Category;
    int32 SelectedRecipe = INDEX_NONE;
    int32 Quantity = 1;
    FSlateBrush Portrait;
    void SetWorkbench(bool bOpen);
    bool bModifying = false;
    enum class EPage { Pack, Crafting, Skills, Gear, Stats, Loot };
    EPage Page = EPage::Crafting;
    int32 TrainingTree = 0;
    TSharedPtr<SSearchBox> TrainingFilterBox;
    TSharedPtr<SScrollBox> TrainingRows;
    FString TrainingSearch;
    int32 SelectedTraining = 0;
    int32 PreviewRank = 1;
    TMap<FName, FSlateBrush> ThemeBrushes;
    TArray<TStrongObjectPtr<UTexture2D>> ThemeTextures;

    TWeakObjectPtr<ASurvivalCharacter> Player;
    FSlateBrush Icons[static_cast<int32>(ESurvivalItem::None)];
};
