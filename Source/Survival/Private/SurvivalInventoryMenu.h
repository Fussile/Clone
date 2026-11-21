#pragma once

#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"
#include "SurvivalVitals.h"
#include "Widgets/SCompoundWidget.h"

class ASurvivalCharacter;
class ASurvivalHUD;

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
    TSharedRef<SWidget> MakeRecipeDetails();
    FText GetIngredientText() const;

    FString SearchText;
    FString Category;
    int32 SelectedRecipe = INDEX_NONE;
    int32 Quantity = 1;
    bool bShowSkills = false;
    int32 SelectedTraining = 0;
    int32 PreviewRank = 1;
    TMap<FName, FSlateBrush> ThemeBrushes;
    TArray<TStrongObjectPtr<UTexture2D>> ThemeTextures;

    TWeakObjectPtr<ASurvivalCharacter> Player;
    FSlateBrush Icons[static_cast<int32>(ESurvivalItem::None)];
};
