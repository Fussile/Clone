#include "SurvivalInventoryMenu.h"

#include "Brushes/SlateColorBrush.h"
#include "Engine/Texture2D.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "SurvivalCharacter.h"
#include "SurvivalHUD.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
const FLinearColor Accent(0.75f, 0.82f, 0.36f);
const FLinearColor Muted(0.45f, 0.52f, 0.47f);
const FLinearColor Backdrop(0.024f, 0.032f, 0.028f, 0.99f);
const FLinearColor PanelColor(0.047f, 0.059f, 0.049f);
const FLinearColor Tile(0.10f, 0.13f, 0.11f);
const FLinearColor Chosen(0.19f, 0.25f, 0.11f);

const FButtonStyle& MenuButtonStyle()
{
    static const FButtonStyle Style = FButtonStyle()
        .SetNormal(FSlateColorBrush(FLinearColor(0.82f, 0.82f, 0.82f)))
        .SetHovered(FSlateColorBrush(FLinearColor::White))
        .SetPressed(FSlateColorBrush(FLinearColor(0.62f, 0.62f, 0.62f)))
        .SetNormalPadding(FMargin(11, 7))
        .SetPressedPadding(FMargin(11, 7));
    return Style;
}

const FKey SlotKeys[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six};
static_assert(UE_ARRAY_COUNT(SlotKeys) == ASurvivalCharacter::HotbarSize);
}

const FSlateBrush* SSurvivalInventoryMenu::FindIcon(ESurvivalItem Item) const
{
    return Item != ESurvivalItem::None ? &Icons[static_cast<int32>(Item)] : nullptr;
}

const FSlateBrush* SSurvivalInventoryMenu::FindThemeIcon(const TCHAR* Name) const
{
    return ThemeBrushes.Find(FName(Name));
}

void SSurvivalInventoryMenu::Construct(const FArguments& Args)
{
    Player = Args._Player;
    for (const TCHAR* Name : {TEXT("Combat"), TEXT("Construction"), TEXT("Athletics"),
         TEXT("Workshop"), TEXT("Backpack"), TEXT("Skills"), TEXT("Scavenging")})
    {
        const FString Path = FString::Printf(TEXT("/Game/Art/Crossroads/Interface/T_UI_%s.T_UI_%s"), Name, Name);
        if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *Path, nullptr, LOAD_NoWarn))
        {
            ThemeTextures.Emplace(Texture);
            FSlateBrush& Brush = ThemeBrushes.Add(FName(Name));
            Brush.SetImageSize(FVector2D(18, 18));
            Brush.SetResourceObject(Texture);
            Brush.DrawAs = ESlateBrushDrawType::Image;
        }
    }
    SelectFirstMatchingRecipe();
    TSharedRef<SUniformGridPanel> Items = SNew(SUniformGridPanel).SlotPadding(FMargin(3.0f));
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Icons); ++Index)
    {
        const ESurvivalItem Item = static_cast<ESurvivalItem>(Index);
        Icons[Index].SetImageSize(FVector2D(36.0, 36.0));
        Icons[Index].SetResourceObject(Args._HUD->GetItemIcon(Item));
        Icons[Index].SetUVRegion(Args._HUD->GetItemIconUV(Item));
        Icons[Index].DrawAs = Icons[Index].GetResourceObject() ? ESlateBrushDrawType::Image : ESlateBrushDrawType::NoDrawType;
        Items->AddSlot(Index % 5, Index / 5)
        [
            SNew(SButton).ButtonStyle(&MenuButtonStyle())
            .HAlign(HAlign_Center)
            .ButtonColorAndOpacity_Lambda([this, Item]()
            {
                return FSlateColor(Player.IsValid() && Player->GetSelectedInventoryItem() == Item ? Chosen : Tile);
            })
            .OnClicked_Lambda([this, Item]()
            {
                if (ASurvivalCharacter* Character = Player.Get())
                {
                    Character->SetSelectedInventoryItem(Item);
                }
                return FReply::Handled();
            })
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                [
                    SNew(SImage).Image(&Icons[Index])
                    .Visibility(Icons[Index].GetResourceObject() ? EVisibility::Visible : EVisibility::Hidden)
                ]
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                [
                    SNew(STextBlock).Text(FText::FromString(SurvivalItemName(Item)))
                ]
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                [
                    SNew(STextBlock).ColorAndOpacity(FSlateColor(Accent))
                    .Text_Lambda([this, Item]()
                    {
                        return FText::AsNumber(Player.IsValid() ? Player->GetItemCount(Item) : 0);
                    })
                ]
            ]
        ];
    }

    TSharedRef<SHorizontalBox> Slots = SNew(SHorizontalBox);
    for (int32 Slot = 0; Slot < ASurvivalCharacter::HotbarSize; ++Slot)
    {
        Slots->AddSlot().FillWidth(1).Padding(2.0f)
        [
            SNew(SButton).ButtonStyle(&MenuButtonStyle())
            .HAlign(HAlign_Center)
            .ButtonColorAndOpacity_Lambda([this, Slot]()
            {
                return FSlateColor(Player.IsValid() && Player->GetActiveHotbarSlot() == Slot ? Chosen : Tile);
            })
            .ToolTipText_Lambda([this, Slot]()
            {
                return FText::FromString(SurvivalItemName(Player.IsValid() ? Player->GetHotbarItem(Slot) : ESurvivalItem::None));
            })
            .OnClicked_Lambda([this, Slot]()
            {
                if (ASurvivalCharacter* Character = Player.Get())
                {
                    Character->ChooseHotbarSlot(Slot);
                }
                return FReply::Handled();
            })
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.0f)
                [
                    SNew(STextBlock).Text(FText::AsNumber(Slot + 1))
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(3.0f)
                [
                    SNew(SImage).Image_Lambda([this, Slot]()
                    {
                        return Player.IsValid() ? FindIcon(Player->GetHotbarItem(Slot)) : nullptr;
                    })
                ]
            ]
        ];
    }

    ChildSlot
    .HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(16.0f)
    [
        SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
        [
            SNew(SBox).WidthOverride(900.0f)
            [
                SNew(SBorder).Padding(FMargin(19.0f))
                .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Backdrop)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 14)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1)
                        [
                            SNew(STextBlock).Text_Lambda([this]()
                            {
                                return FText::FromString(bShowSkills ? TEXT("Skills") : TEXT("Crafting"));
                            })
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 24)).ColorAndOpacity(FSlateColor(Accent))
                        ]
                        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)
                        [
                            SNew(SButton).ButtonStyle(&MenuButtonStyle()).ButtonColorAndOpacity(Tile)
                            .OnClicked_Lambda([this]()
                            {
                                bShowSkills = !bShowSkills;
                                return FReply::Handled();
                            })
                            [
                                SNew(SHorizontalBox)
                                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
                                [
                                    SNew(SImage).Image_Lambda([this]() { return FindThemeIcon(bShowSkills ? TEXT("Backpack") : TEXT("Skills")); })
                                    .ColorAndOpacity(Accent)
                                ]
                                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                                [
                                    SNew(STextBlock).Text_Lambda([this]()
                                    {
                                        return FText::FromString(bShowSkills ? TEXT("Back to crafting") : TEXT("View skills"));
                                    })
                                ]
                            ]
                        ]
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(SButton).ButtonStyle(&MenuButtonStyle()).ButtonColorAndOpacity(Tile).Text(FText::FromString(TEXT("Close [I]")))
                            .OnClicked_Lambda([this]()
                            {
                                if (ASurvivalCharacter* Character = Player.Get())
                                {
                                    Character->ToggleInventory();
                                }
                                return FReply::Handled();
                            })
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        .Visibility_Lambda([this]() { return bShowSkills ? EVisibility::Collapsed : EVisibility::Visible; })
                        + SHorizontalBox::Slot().FillWidth(0.44f).Padding(0, 0, 20, 0)[MakeRecipeBrowser()]
                        + SHorizontalBox::Slot().FillWidth(0.56f)[MakeRecipeDetails()]
                    ]
                    + SVerticalBox::Slot().AutoHeight()[MakeSkillsPanel()]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SVerticalBox)
                        .Visibility_Lambda([this]() { return bShowSkills ? EVisibility::Collapsed : EVisibility::Visible; })
                        + SVerticalBox::Slot().AutoHeight().Padding(3, 16, 3, 5)
                        [
                            SNew(STextBlock).Text(FText::FromString(TEXT("Backpack")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 15)).ColorAndOpacity(FSlateColor(Accent))
                        ]
                        + SVerticalBox::Slot().AutoHeight()[Items]
                        + SVerticalBox::Slot().AutoHeight().Padding(3, 7, 3, 8)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)
                            [
                                SNew(SButton).ButtonStyle(&MenuButtonStyle()).ButtonColorAndOpacity(Tile).Text(FText::FromString(TEXT("Use selected")))
                                .IsEnabled_Lambda([this]()
                                {
                                    return Player.IsValid() && Player->CanUseItem(Player->GetSelectedInventoryItem());
                                })
                                .OnClicked_Lambda([this]()
                                {
                                    if (ASurvivalCharacter* Character = Player.Get())
                                    {
                                        Character->UseItem(Character->GetSelectedInventoryItem());
                                    }
                                    return FReply::Handled();
                                })
                            ]
                            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                            [
                                SNew(STextBlock).Text(FText::FromString(TEXT("Select an item, then click a slot or press 1-6 to assign it.")))
                                .ColorAndOpacity(FSlateColor(Accent))
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight()[Slots]
                    ]
                ]
            ]
        ]
    ];
}

TSharedRef<SWidget> SSurvivalInventoryMenu::MakeSkillsPanel()
{
    TSharedRef<SScrollBox> Rows = SNew(SScrollBox);
    const TCHAR* GroupNames[] = {TEXT("Learn by doing"), TEXT("Knowledge"), TEXT("Future systems")};
    for (int32 Group = 0; Group < UE_ARRAY_COUNT(GroupNames); ++Group)
    {
        Rows->AddSlot().Padding(2, Group == 0 ? 0 : 14, 2, 6)
        [
            SNew(STextBlock).Text(FText::FromString(GroupNames[Group]))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11)).ColorAndOpacity(FSlateColor(Muted))
            .Visibility_Lambda([this]()
            {
                return TrainingSearch.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
            })
        ];
        for (int32 Index = 0; Index < FSurvivalProgression::EntryCount; ++Index)
        {
            const auto Entry = static_cast<ETraining>(Index);
            const FTrainingInfo& Info = GetTrainingInfo(Entry);
            const int32 EntryGroup = !Info.bAvailable ? 2 : Info.IsPractice() ? 0 : 1;
            if (EntryGroup != Group)
            {
                continue;
            }
            Rows->AddSlot().Padding(0, 0, 6, 4)
            [
                SNew(SButton).ButtonStyle(&MenuButtonStyle()).HAlign(HAlign_Fill).ContentPadding(FMargin(9, 7))
                .ButtonColorAndOpacity_Lambda([this, Index]() { return FSlateColor(SelectedTraining == Index ? Chosen : Tile); })
                .Visibility_Lambda([this, Entry]()
                {
                    const FTrainingInfo& Training = GetTrainingInfo(Entry);
                    return TrainingSearch.IsEmpty() || FString(Training.Name).Contains(TrainingSearch)
                        || FString(Training.Category).Contains(TrainingSearch)
                        ? EVisibility::Visible : EVisibility::Collapsed;
                })
                .OnClicked_Lambda([this, Index, Entry]()
                {
                    SelectedTraining = Index;
                    const FTrainingInfo& Training = GetTrainingInfo(Entry);
                    PreviewRank = Player.IsValid()
                        ? FMath::Min(Player->GetProgression().Rank(Entry) + 1, Training.MaxRank) : 1;
                    return FReply::Handled();
                })
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 9, 0)
                    [
                        SNew(SImage).Image(FindThemeIcon(Info.Icon)).ColorAndOpacity(Accent)
                    ]
                    + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                    [
                        SNew(STextBlock).Text(FText::FromString(Info.Name))
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0)
                    [
                        SNew(STextBlock).ColorAndOpacity(FSlateColor(Muted))
                        .Text_Lambda([this, Entry]()
                        {
                            const FTrainingInfo& Training = GetTrainingInfo(Entry);
                            return Training.bAvailable && Player.IsValid()
                                ? FText::FromString(FString::Printf(TEXT("%d/%d"), Player->GetProgression().Rank(Entry), Training.MaxRank))
                                : FText::FromString(TEXT("Planned"));
                        })
                    ]
                ]
            ];
        }
    }

    const auto SelectedEntry = [this]() { return static_cast<ETraining>(SelectedTraining); };
    const auto IsKnowledge = [SelectedEntry]() { return !GetTrainingInfo(SelectedEntry()).IsPractice(); };
    return SNew(SVerticalBox)
        .Visibility_Lambda([this]() { return bShowSkills ? EVisibility::Visible : EVisibility::Collapsed; })
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
        [
            SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(PanelColor)
            .Padding(FMargin(14, 9))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
                [
                    SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
                    .ColorAndOpacity(FSlateColor(Accent)).Text_Lambda([this]()
                    {
                        if (!Player.IsValid()) { return FText::GetEmpty(); }
                        const FSurvivalProgression& Progress = Player->GetProgression();
                        return FText::FromString(FString::Printf(TEXT("Level %d     •     %d points available     •     %d / %d XP"),
                            Progress.CharacterLevel, Progress.Points, Progress.CharacterXP(), Progress.LevelXPRequired()));
                    })
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SProgressBar).FillColorAndOpacity(Accent).Percent_Lambda([this]() -> TOptional<float>
                    {
                        if (!Player.IsValid()) { return 0.0f; }
                        const FSurvivalProgression& Progress = Player->GetProgression();
                        return static_cast<float>(Progress.CharacterXP()) / Progress.LevelXPRequired();
                    })
                ]
            ]
        ]
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SBox).HeightOverride(388)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(0.40f).Padding(0, 0, 12, 0)
                [
                    SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(PanelColor)
                    .Padding(FMargin(10))
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
                        [
                            SNew(SSearchBox).HintText(FText::FromString(TEXT("Search skills or categories")))
                            .OnTextChanged_Lambda([this](const FText& Text) { TrainingSearch = Text.ToString().TrimStartAndEnd(); })
                        ]
                        + SVerticalBox::Slot().FillHeight(1)[Rows]
                    ]
                ]
                + SHorizontalBox::Slot().FillWidth(0.60f)
                [
                    SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(PanelColor)
                    .Padding(FMargin(20))
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
                        [
                            SNew(STextBlock).ColorAndOpacity(FSlateColor(Accent))
                            .Text_Lambda([SelectedEntry]()
                            {
                                const FTrainingInfo& Info = GetTrainingInfo(SelectedEntry());
                                return FText::FromString(FString::Printf(TEXT("%s  /  %s"), Info.Category,
                                    Info.IsPractice() ? TEXT("Learn by doing") : TEXT("Knowledge")));
                            })
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 18)
                        [
                            SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 26))
                            .Text_Lambda([SelectedEntry]()
                            {
                                return FText::FromString(GetTrainingInfo(SelectedEntry()).Name);
                            })
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
                        [
                            SNew(STextBlock).AutoWrapText(true).Text_Lambda([SelectedEntry]()
                            {
                                return FText::FromString(GetTrainingInfo(SelectedEntry()).Description);
                            })
                        ]
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(STextBlock).ColorAndOpacity(FSlateColor(Muted)).AutoWrapText(true)
                            .Text_Lambda([this, SelectedEntry]()
                            {
                                if (!Player.IsValid()) { return FText::GetEmpty(); }
                                const FSurvivalProgression& Progress = Player->GetProgression();
                                const ETraining Entry = SelectedEntry();
                                const FTrainingInfo& Info = GetTrainingInfo(Entry);
                                const int32 Rank = Progress.Rank(Entry);
                                if (Info.IsPractice())
                                {
                                    if (!Info.bAvailable)
                                    {
                                        return FText::FromString(FString::Printf(TEXT("Available when its activity is added.  /  %d ranks"), Info.MaxRank));
                                    }
                                    return Rank == Info.MaxRank ? FText::FromString(TEXT("Maximum rank"))
                                        : FText::FromString(FString::Printf(TEXT("Rank %d  /  %d of %d practice XP"),
                                            Rank, Progress.Practice(Entry), Progress.PracticeRequired(Entry)));
                                }
                                FString Requirements;
                                if (!Info.bAvailable)
                                {
                                    Requirements = TEXT("Available when its system is added.\n");
                                }
                                if (PreviewRank <= Rank && Info.bAvailable)
                                {
                                    return FText::FromString(TEXT("Rank learned"));
                                }
                                const int32 Cost = Info.bAvailable ? Progress.PointCostThrough(Entry, PreviewRank) : Info.PointCost(PreviewRank);
                                Requirements += FString::Printf(TEXT("%d points%s"), Cost,
                                    Info.bAvailable ? TEXT(" to learn") : TEXT(" for this rank"));
                                for (const FTrainingGate& Gate : GetTrainingGates())
                                {
                                    if (Gate.Entry == Entry && Gate.Rank == PreviewRank)
                                    {
                                        if (Gate.RequiredEntry == ETraining::Count)
                                        {
                                            Requirements += FString::Printf(TEXT("\nCharacter level %d"), Gate.RequiredRank);
                                        }
                                        else
                                        {
                                            Requirements += FString::Printf(TEXT("\n%s %d"),
                                                GetTrainingInfo(Gate.RequiredEntry).Name, Gate.RequiredRank);
                                        }
                                    }
                                }
                                return FText::FromString(Requirements);
                            })
                        ]
                        + SVerticalBox::Slot().FillHeight(1)
                        [
                            SNew(SSpacer)
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
                        [
                            SNew(SHorizontalBox)
                            .Visibility_Lambda([IsKnowledge]() { return IsKnowledge() ? EVisibility::Visible : EVisibility::Collapsed; })
                            + SHorizontalBox::Slot().AutoWidth()
                            [
                                SNew(SButton).ButtonStyle(&MenuButtonStyle()).ButtonColorAndOpacity(Tile)
                                .Text(FText::FromString(TEXT("−")))
                                .IsEnabled_Lambda([this]() { return PreviewRank > 1; })
                                .OnClicked_Lambda([this]() { --PreviewRank; return FReply::Handled(); })
                            ]
                            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(10, 0)
                            [
                                SNew(STextBlock).Text_Lambda([this, IsKnowledge, SelectedEntry]()
                                {
                                    return IsKnowledge() ? FText::FromString(FString::Printf(TEXT("Rank %d of %d"),
                                        PreviewRank, GetTrainingInfo(SelectedEntry()).MaxRank)) : FText::GetEmpty();
                                })
                            ]
                            + SHorizontalBox::Slot().AutoWidth()
                            [
                                SNew(SButton).ButtonStyle(&MenuButtonStyle()).ButtonColorAndOpacity(Tile)
                                .Text(FText::FromString(TEXT("+")))
                                .IsEnabled_Lambda([this, IsKnowledge, SelectedEntry]()
                                {
                                    return IsKnowledge() && PreviewRank < GetTrainingInfo(SelectedEntry()).MaxRank;
                                })
                                .OnClicked_Lambda([this]() { ++PreviewRank; return FReply::Handled(); })
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(SButton).ButtonStyle(&MenuButtonStyle()).ButtonColorAndOpacity(Chosen)
                            .Visibility_Lambda([IsKnowledge]() { return IsKnowledge() ? EVisibility::Visible : EVisibility::Collapsed; })
                            .Text_Lambda([this, IsKnowledge, SelectedEntry]()
                            {
                                if (!Player.IsValid() || !IsKnowledge()) { return FText::GetEmpty(); }
                                const auto Entry = SelectedEntry();
                                if (!GetTrainingInfo(Entry).bAvailable) { return FText::FromString(TEXT("Unavailable")); }
                                const int32 Owned = Player->GetProgression().Rank(Entry);
                                if (PreviewRank <= Owned) { return FText::FromString(TEXT("Learned")); }
                                return FText::FromString(PreviewRank == Owned + 1 ? TEXT("Learn rank") : TEXT("Learn up to this rank"));
                            })
                            .IsEnabled_Lambda([this, IsKnowledge, SelectedEntry]()
                            {
                                return Player.IsValid() && IsKnowledge()
                                    && Player->GetProgression().CanBuyThrough(SelectedEntry(), PreviewRank);
                            })
                            .OnClicked_Lambda([this, SelectedEntry]()
                            {
                                if (ASurvivalCharacter* Character = Player.Get())
                                {
                                    Character->BuyTraining(SelectedEntry(), PreviewRank);
                                }
                                return FReply::Handled();
                            })
                        ]
                    ]
                ]
            ]
        ];
}

bool SSurvivalInventoryMenu::MatchesFilters(const FSurvivalRecipe& Recipe) const
{
    return (Category.IsEmpty() || Category == Recipe.Category)
        && (SearchText.IsEmpty() || FString(SurvivalItemName(Recipe.Output)).Contains(SearchText));
}

void SSurvivalInventoryMenu::SelectFirstMatchingRecipe()
{
    const TConstArrayView<FSurvivalRecipe> Recipes = GetSurvivalRecipes();
    if (!Recipes.IsValidIndex(SelectedRecipe) || !MatchesFilters(Recipes[SelectedRecipe]))
    {
        SelectedRecipe = INDEX_NONE;
        for (int32 Index = 0; Index < Recipes.Num(); ++Index)
        {
            if (MatchesFilters(Recipes[Index]))
            {
                SelectedRecipe = Index;
                break;
            }
        }
    }
}

TSharedRef<SWidget> SSurvivalInventoryMenu::MakeRecipeBrowser()
{
    TSharedRef<SHorizontalBox> Categories = SNew(SHorizontalBox);
    for (const TCHAR* Label : {TEXT("All"), TEXT("Medical"), TEXT("Building")})
    {
        Categories->AddSlot().FillWidth(1).Padding(0, 0, 4, 0)
        [
            SNew(SButton).HAlign(HAlign_Center).Text(FText::FromString(Label))
            .ButtonColorAndOpacity_Lambda([this, Label]()
            {
                return FSlateColor(Category == Label || (Category.IsEmpty() && FCString::Strcmp(Label, TEXT("All")) == 0)
                    ? Chosen : Tile);
            })
            .OnClicked_Lambda([this, Label]()
            {
                Category = FCString::Strcmp(Label, TEXT("All")) == 0 ? TEXT("") : Label;
                SelectFirstMatchingRecipe();
                return FReply::Handled();
            })
        ];
    }

    TSharedRef<SScrollBox> Rows = SNew(SScrollBox);
    const TConstArrayView<FSurvivalRecipe> Recipes = GetSurvivalRecipes();
    for (int32 Index = 0; Index < Recipes.Num(); ++Index)
    {
        const ESurvivalItem Item = Recipes[Index].Output;
        Rows->AddSlot().Padding(0, 3)
        [
            SNew(SButton).ButtonColorAndOpacity_Lambda([this, Index]()
            {
                return FSlateColor(SelectedRecipe == Index ? Chosen : Tile);
            })
            .Visibility_Lambda([this, Index]()
            {
                return MatchesFilters(GetSurvivalRecipes()[Index]) ? EVisibility::Visible : EVisibility::Collapsed;
            })
            .OnClicked_Lambda([this, Index]()
            {
                SelectedRecipe = Index;
                return FReply::Handled();
            })
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)[SNew(SImage).Image(FindIcon(Item))]
                + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                [
                    SNew(STextBlock).Text(FText::FromString(SurvivalItemName(Item)))
                ]
            ]
        ];
    }

    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
        [
            SNew(SSearchBox).HintText(FText::FromString(TEXT("Find recipe")))
            .OnTextChanged_Lambda([this](const FText& Text)
            {
                SearchText = Text.ToString().TrimStartAndEnd();
                SelectFirstMatchingRecipe();
            })
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)[Categories]
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SBox).HeightOverride(168.0f)[Rows]
        ];
}

FText SSurvivalInventoryMenu::GetIngredientText() const
{
    const TConstArrayView<FSurvivalRecipe> Recipes = GetSurvivalRecipes();
    if (!Recipes.IsValidIndex(SelectedRecipe))
    {
        return FText::GetEmpty();
    }

    FString Ingredients;
    for (const FSurvivalIngredient& Ingredient : Recipes[SelectedRecipe].Ingredients)
    {
        const int32 Held = Player.IsValid() ? Player->GetItemCount(Ingredient.Item) : 0;
        Ingredients += FString::Printf(TEXT("%s   %d / %d\n"), SurvivalItemName(Ingredient.Item), Held, Ingredient.Count * Quantity);
    }
    return FText::FromString(Ingredients.TrimEnd());
}

TSharedRef<SWidget> SSurvivalInventoryMenu::MakeRecipeDetails()
{
    return SNew(SBorder).Padding(FMargin(12.0f))
        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(FLinearColor(0.08f, 0.14f, 0.17f))
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)
            [
                SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
                .Text_Lambda([this]()
                {
                    const TConstArrayView<FSurvivalRecipe> Recipes = GetSurvivalRecipes();
                    return FText::FromString(Recipes.IsValidIndex(SelectedRecipe)
                        ? SurvivalItemName(Recipes[SelectedRecipe].Output) : TEXT("No matching recipes"));
                })
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
            [
                SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]()
                {
                    const TConstArrayView<FSurvivalRecipe> Recipes = GetSurvivalRecipes();
                    return Recipes.IsValidIndex(SelectedRecipe)
                        ? FText::FromString(Recipes[SelectedRecipe].Description) : FText::GetEmpty();
                })
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(STextBlock).Text(FText::FromString(TEXT("Ingredients   held / required"))).ColorAndOpacity(FSlateColor(Accent))
                .Visibility_Lambda([this]() { return SelectedRecipe == INDEX_NONE ? EVisibility::Collapsed : EVisibility::Visible; })
            ]
            + SVerticalBox::Slot().FillHeight(1).Padding(0, 6, 0, 10)
            [
                SNew(SScrollBox)
                + SScrollBox::Slot()[SNew(STextBlock).Text(this, &SSurvivalInventoryMenu::GetIngredientText)]
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SHorizontalBox)
                .Visibility_Lambda([this]() { return SelectedRecipe == INDEX_NONE ? EVisibility::Collapsed : EVisibility::Visible; })
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)
                [
                    SNew(STextBlock).Text(FText::FromString(TEXT("Quantity")))
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
                [
                    SNew(SBox).WidthOverride(64.0f)
                    [
                        SNew(SSpinBox<int32>).MinValue(1).MaxValue(99).MaxSliderValue(20).Value(1)
                        .OnValueChanged_Lambda([this](int32 Value) { Quantity = Value; })
                    ]
                ]
                + SHorizontalBox::Slot().FillWidth(1)
                [
                    SNew(SButton).HAlign(HAlign_Center).Text(FText::FromString(TEXT("Craft")))
                    .IsEnabled_Lambda([this]() { return Player.IsValid() && Player->CanCraftRecipe(SelectedRecipe, Quantity); })
                    .OnClicked_Lambda([this]()
                    {
                        if (ASurvivalCharacter* Character = Player.Get())
                        {
                            Character->CraftRecipe(SelectedRecipe, Quantity);
                        }
                        return FReply::Handled();
                    })
                ]
            ]
        ];
}

FReply SSurvivalInventoryMenu::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (ASurvivalCharacter* Character = Player.Get())
    {
        if (Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::I)
        {
            if (!Event.IsRepeat())
            {
                Character->ToggleInventory();
            }
            return FReply::Handled();
        }
        for (int32 Slot = 0; Slot < UE_ARRAY_COUNT(SlotKeys); ++Slot)
        {
            if (Event.GetKey() == SlotKeys[Slot])
            {
                Character->ChooseHotbarSlot(Slot);
                return FReply::Handled();
            }
        }
    }
    return SCompoundWidget::OnKeyDown(Geometry, Event);
}
