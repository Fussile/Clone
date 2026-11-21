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
const FLinearColor Accent(0.87f, 0.45f, 0.25f);
const FLinearColor Ink(0.91f, 0.87f, 0.80f);
const FLinearColor Muted(0.62f, 0.64f, 0.62f);
const FLinearColor Backdrop(0.019f, 0.024f, 0.027f, 0.97f);
const FLinearColor PanelColor(0.043f, 0.050f, 0.054f);
const FLinearColor Tile(0.081f, 0.093f, 0.101f);
const FLinearColor Chosen(0.22f, 0.11f, 0.066f);

struct FTrainingTree { const TCHAR* Label; const TCHAR* Category; const TCHAR* Icon; };
constexpr FTrainingTree TrainingTrees[] = {
    {TEXT("Practice"), nullptr, TEXT("Athletics")},
    {TEXT("Survival"), TEXT("Survival"), TEXT("Athletics")},
    {TEXT("Scavenging"), TEXT("Scavenging"), TEXT("Scavenging")},
    {TEXT("Construction"), TEXT("Tools and construction"), TEXT("Construction")},
    {TEXT("Melee"), TEXT("Combat"), TEXT("Combat")},
    {TEXT("Firearms"), TEXT("Firearms"), TEXT("Combat")},
    {TEXT("Technology"), TEXT("Technology"), TEXT("Workshop")},
    {TEXT("Finishes"), TEXT("Finishes"), TEXT("Workshop")},
};

const FSlateBrush* WhiteBrush()
{
    return FCoreStyle::Get().GetBrush("WhiteBrush");
}

TSharedRef<STextBlock> SectionLabel(const TCHAR* Label)
{
    return SNew(STextBlock).Text(FText::FromString(Label))
        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10)).ColorAndOpacity(FSlateColor(Muted));
}

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
                    .Justification(ETextJustify::Center).AutoWrapText(true)
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

    TSharedRef<SHorizontalBox> Tabs = SNew(SHorizontalBox);
    const TCHAR* PageNames[] = {TEXT("Backpack"), TEXT("Crafting"), TEXT("Skills")};
    const TCHAR* PageIcons[] = {TEXT("Backpack"), TEXT("Workshop"), TEXT("Skills")};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(PageNames); ++Index)
    {
        const auto Tab = static_cast<EPage>(Index);
        Tabs->AddSlot().AutoWidth().Padding(0, 0, 5, 0)
        [
            SNew(SButton).ButtonStyle(&MenuButtonStyle()).ContentPadding(FMargin(15, 10))
            .ButtonColorAndOpacity_Lambda([this, Tab]() { return FSlateColor(Page == Tab ? Chosen : Tile); })
            .OnClicked_Lambda([this, Tab]() { Page = Tab; return FReply::Handled(); })
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
                [SNew(SImage).Image(FindThemeIcon(PageIcons[Index])).ColorAndOpacity(Accent)]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(STextBlock).Text(FText::FromString(PageNames[Index])).ColorAndOpacity(FSlateColor(Ink))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
                ]
            ]
        ];
    }

    ChildSlot
    .HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(24.0f)
    [
        SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
        [
            SNew(SBox).WidthOverride(1160.0f).HeightOverride(654.0f)
            [
                SNew(SBorder).Padding(FMargin(18))
                .BorderImage(WhiteBrush()).BorderBackgroundColor(Backdrop)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 16)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2, 0, 25, 0)
                        [
                            SNew(STextBlock).Text(FText::FromString(TEXT("CROSSROADS"))).ColorAndOpacity(FSlateColor(Ink))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 21))
                        ]
                        + SHorizontalBox::Slot().FillWidth(1)[Tabs]
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(SButton).ButtonStyle(&MenuButtonStyle()).ButtonColorAndOpacity(Tile)
                            .Text(FText::FromString(TEXT("Close   Esc")))
                            .OnClicked_Lambda([this]()
                            {
                                if (ASurvivalCharacter* Character = Player.Get()) { Character->ToggleInventory(); }
                                return FReply::Handled();
                            })
                        ]
                    ]
                    + SVerticalBox::Slot().FillHeight(1)
                    [
                        SNew(SHorizontalBox)
                        .Visibility_Lambda([this]() { return Page == EPage::Crafting ? EVisibility::Visible : EVisibility::Collapsed; })
                        + SHorizontalBox::Slot().FillWidth(.40f).Padding(0, 0, 12, 0)
                        [
                            SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(PanelColor).Padding(FMargin(16))
                            [MakeRecipeBrowser()]
                        ]
                        + SHorizontalBox::Slot().FillWidth(.60f)[MakeRecipeDetails()]
                    ]
                    + SVerticalBox::Slot().FillHeight(1)[MakeSkillsPanel()]
                    + SVerticalBox::Slot().FillHeight(1)
                    [
                        SNew(SHorizontalBox)
                        .Visibility_Lambda([this]() { return Page == EPage::Pack ? EVisibility::Visible : EVisibility::Collapsed; })
                        + SHorizontalBox::Slot().FillWidth(.73f).Padding(0, 0, 12, 0)
                        [
                            SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(PanelColor).Padding(FMargin(14))
                            [
                                SNew(SVerticalBox)
                                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[SectionLabel(TEXT("CARRIED ITEMS"))]
                                + SVerticalBox::Slot().FillHeight(1)[Items]
                                + SVerticalBox::Slot().AutoHeight().Padding(0, 16, 0, 7)[SectionLabel(TEXT("QUICK SLOTS"))]
                                + SVerticalBox::Slot().AutoHeight()[Slots]
                            ]
                        ]
                        + SHorizontalBox::Slot().FillWidth(.27f)
                        [
                            SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(PanelColor).Padding(FMargin(20))
                            [
                                SNew(SVerticalBox)
                                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(0, 0, 0, 18)
                                [
                                    SNew(SImage).DesiredSizeOverride(FVector2D(62, 62))
                                    .Image_Lambda([this]() { return Player.IsValid() ? FindIcon(Player->GetSelectedInventoryItem()) : nullptr; })
                                ]
                                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
                                [
                                    SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FSlateColor(Ink))
                                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 23))
                                    .Text_Lambda([this]()
                                    {
                                        return Player.IsValid() ? FText::FromString(SurvivalItemName(Player->GetSelectedInventoryItem())) : FText::GetEmpty();
                                    })
                                ]
                                + SVerticalBox::Slot().AutoHeight()
                                [
                                    SNew(STextBlock).ColorAndOpacity(FSlateColor(Muted)).Text_Lambda([this]()
                                    {
                                        return Player.IsValid() ? FText::FromString(FString::Printf(TEXT("%d carried"),
                                            Player->GetItemCount(Player->GetSelectedInventoryItem()))) : FText::GetEmpty();
                                    })
                                ]
                                + SVerticalBox::Slot().FillHeight(1)[SNew(SSpacer)]
                                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
                                [
                                    SNew(SButton).ButtonStyle(&MenuButtonStyle()).ButtonColorAndOpacity(Chosen)
                                    .HAlign(HAlign_Center).Text(FText::FromString(TEXT("Use selected item")))
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
                                + SVerticalBox::Slot().AutoHeight()
                                [
                                    SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FSlateColor(Muted))
                                    .Text(FText::FromString(TEXT("Click a quick slot or press 1–6 to assign this item.")))
                                ]
                            ]
                        ]
                    ]
                ]
            ]
        ]
    ];
}

bool SSurvivalInventoryMenu::MatchesTraining(ETraining Entry) const
{
    const FTrainingInfo& Info = GetTrainingInfo(Entry);
    const TCHAR* Tree = TrainingTrees[TrainingTree].Category;
    const bool bInTree = Tree ? !Info.IsPractice() && FCString::Strcmp(Tree, Info.Category) == 0 : Info.IsPractice();
    return bInTree && (TrainingSearch.IsEmpty() || FString(Info.Name).Contains(TrainingSearch)
        || FString(Info.Description).Contains(TrainingSearch));
}

void SSurvivalInventoryMenu::SelectTraining(int32 Index)
{
    SelectedTraining = Index;
    if (Index != INDEX_NONE)
    {
        const auto Entry = static_cast<ETraining>(Index);
        const int32 Rank = Player.IsValid() ? Player->GetProgression().Rank(Entry) : 0;
        PreviewRank = FMath::Min(Rank + 1, GetTrainingInfo(Entry).MaxRank);
    }
}

void SSurvivalInventoryMenu::SelectFirstTraining()
{
    SelectTraining(INDEX_NONE);
    for (int32 Index = 0; Index < FSurvivalProgression::EntryCount; ++Index)
    {
        const auto Entry = static_cast<ETraining>(Index);
        if (MatchesTraining(Entry))
        {
            if (SelectedTraining == INDEX_NONE)
            {
                SelectTraining(Index);
            }
            if (GetTrainingInfo(Entry).bAvailable)
            {
                SelectTraining(Index);
                break;
            }
        }
    }
    TrainingRows->ScrollToStart();
}

FText SSurvivalInventoryMenu::TrainingRequirements() const
{
    if (!Player.IsValid() || SelectedTraining == INDEX_NONE)
    {
        return FText::GetEmpty();
    }
    const auto Entry = static_cast<ETraining>(SelectedTraining);
    const FTrainingInfo& Info = GetTrainingInfo(Entry);
    const FSurvivalProgression& Progress = Player->GetProgression();
    const int32 Rank = Progress.Rank(Entry);
    if (Info.IsPractice())
    {
        if (!Info.bAvailable)
        {
            return FText::AsNumber(Info.MaxRank);
        }
        return FText::FromString(Rank == Info.MaxRank ? FString(TEXT("Maximum rank"))
            : FString::Printf(TEXT("%d / %d practice XP to next rank"), Progress.Practice(Entry), Progress.PracticeRequired(Entry)));
    }
    if (PreviewRank <= Rank)
    {
        return FText::FromString(TEXT("Rank learned"));
    }

    FString Text = FString::Printf(TEXT("%d skill points"), Progress.PointCostThrough(Entry, PreviewRank));
    for (const FTrainingGate& Gate : GetTrainingGates())
    {
        if (Gate.Entry == Entry && Gate.Rank == PreviewRank)
        {
            Text += Gate.RequiredEntry == ETraining::Count
                ? FString::Printf(TEXT("\nCharacter level %d"), Gate.RequiredRank)
                : FString::Printf(TEXT("\n%s rank %d"), GetTrainingInfo(Gate.RequiredEntry).Name, Gate.RequiredRank);
        }
    }
    return FText::FromString(Text);
}

TSharedRef<SWidget> SSurvivalInventoryMenu::MakeSkillsPanel()
{
    TSharedRef<SVerticalBox> Navigation = SNew(SVerticalBox);
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(TrainingTrees); ++Index)
    {
        if (Index == 1)
        {
            Navigation->AddSlot().AutoHeight().Padding(10, 18, 0, 8)
            [
                SNew(STextBlock).Text(FText::FromString(TEXT("KNOWLEDGE TREES")))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9)).ColorAndOpacity(FSlateColor(Muted))
            ];
        }
        Navigation->AddSlot().AutoHeight().Padding(0, 0, 0, 4)
        [
            SNew(SButton).ButtonStyle(&MenuButtonStyle()).HAlign(HAlign_Left)
            .ButtonColorAndOpacity_Lambda([this, Index]() { return FSlateColor(TrainingTree == Index ? Chosen : Tile); })
            .OnClicked_Lambda([this, Index]()
            {
                TrainingTree = Index;
                TrainingSearch.Reset();
                TrainingFilterBox->SetText(FText::GetEmpty());
                SelectFirstTraining();
                return FReply::Handled();
            })
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 9, 0)
                [SNew(SImage).Image(FindThemeIcon(TrainingTrees[Index].Icon)).ColorAndOpacity(Accent)]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [SNew(STextBlock).Text(FText::FromString(TrainingTrees[Index].Label))]
            ]
        ];
    }

    SAssignNew(TrainingRows, SScrollBox);
    for (const bool bAvailable : {true, false})
    {
        for (int32 Index = 0; Index < FSurvivalProgression::EntryCount; ++Index)
        {
            const auto Entry = static_cast<ETraining>(Index);
            const FTrainingInfo& Info = GetTrainingInfo(Entry);
            if (Info.bAvailable != bAvailable)
            {
                continue;
            }
            TrainingRows->AddSlot().Padding(0, 0, 5, 3)
            [
                SNew(SButton).ButtonStyle(&MenuButtonStyle()).HAlign(HAlign_Fill).ContentPadding(FMargin(10, 9))
                .Visibility_Lambda([this, Entry]() { return MatchesTraining(Entry) ? EVisibility::Visible : EVisibility::Collapsed; })
                .ButtonColorAndOpacity_Lambda([this, Index]() { return FSlateColor(SelectedTraining == Index ? Chosen : Tile); })
                .OnClicked_Lambda([this, Index]() { SelectTraining(Index); return FReply::Handled(); })
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 9, 0)
                    [SNew(SImage).Image(FindThemeIcon(Info.Icon)).ColorAndOpacity(bAvailable ? Accent : Muted)]
                    + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                    [
                        SNew(STextBlock).Text(FText::FromString(Info.Name))
                        .ColorAndOpacity(FSlateColor(bAvailable ? Ink : Muted))
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        SNew(SBox).WidthOverride(57)
                        [
                            SNew(STextBlock).ColorAndOpacity(FSlateColor(Muted))
                            .Text_Lambda([this, Entry]()
                            {
                                const int32 Rank = Player.IsValid() ? Player->GetProgression().Rank(Entry) : 0;
                                return FText::FromString(FString::Printf(TEXT("%d / %d"), Rank, GetTrainingInfo(Entry).MaxRank));
                            })
                        ]
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        SNew(SBox).WidthOverride(64)
                        [
                            SNew(STextBlock).ColorAndOpacity(FSlateColor(Muted))
                            .Text_Lambda([this, Entry]()
                            {
                                const FTrainingInfo& Training = GetTrainingInfo(Entry);
                                if (!Training.bAvailable)
                                {
                                    return FText::FromString(TEXT("Planned"));
                                }
                                if (!Player.IsValid() || Training.IsPractice())
                                {
                                    return FText::FromString(TEXT("Practice"));
                                }
                                const int32 Rank = Player->GetProgression().Rank(Entry);
                                return Rank >= Training.MaxRank ? FText::FromString(TEXT("Learned"))
                                    : FText::FromString(FString::Printf(TEXT("%d pts"), Training.PointCost(Rank + 1)));
                            })
                        ]
                    ]
                ]
            ];
        }
    }

    return SNew(SVerticalBox)
        .Visibility_Lambda([this]() { return Page == EPage::Skills ? EVisibility::Visible : EVisibility::Collapsed; })
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
        [
            SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(PanelColor).Padding(FMargin(15, 10))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
                [
                    SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 13)).ColorAndOpacity(FSlateColor(Ink))
                    .Text_Lambda([this]()
                    {
                        if (!Player.IsValid()) { return FText::GetEmpty(); }
                        const FSurvivalProgression& Progress = Player->GetProgression();
                        return FText::FromString(FString::Printf(TEXT("Level %d     /     %d skill points     /     %d of %d XP"),
                            Progress.CharacterLevel, Progress.Points, Progress.CharacterXP(), Progress.LevelXPRequired()));
                    })
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SBox).HeightOverride(5)
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
        ]
        + SVerticalBox::Slot().FillHeight(1)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)
            [
                SNew(SBox).WidthOverride(170)
                [
                    SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(PanelColor).Padding(FMargin(9))
                    [Navigation]
                ]
            ]
            + SHorizontalBox::Slot().FillWidth(.55f).Padding(0, 0, 12, 0)
            [
                SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(PanelColor).Padding(FMargin(12))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
                    [
                        SAssignNew(TrainingFilterBox, SSearchBox).HintText(FText::FromString(TEXT("Filter this tree")))
                        .OnTextChanged_Lambda([this](const FText& Value)
                        {
                            TrainingSearch = Value.ToString().TrimStartAndEnd();
                            SelectFirstTraining();
                        })
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(10, 0, 15, 7)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1)[SectionLabel(TEXT("SKILL"))]
                        + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(57)[SectionLabel(TEXT("RANK"))]]
                        + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(64)[SectionLabel(TEXT("NEXT"))]]
                    ]
                    + SVerticalBox::Slot().FillHeight(1)[TrainingRows.ToSharedRef()]
                ]
            ]
            + SHorizontalBox::Slot().FillWidth(.45f)[MakeTrainingDetails()]
        ];
}

TSharedRef<SWidget> SSurvivalInventoryMenu::MakeTrainingDetails()
{
    const auto Entry = [this]() { return static_cast<ETraining>(SelectedTraining); };
    const auto HasSelection = [this]() { return SelectedTraining != INDEX_NONE; };
    const auto IsKnowledge = [HasSelection, Entry]() { return HasSelection() && !GetTrainingInfo(Entry()).IsPractice(); };
    return SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(PanelColor).Padding(FMargin(20))
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
        [
            SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 10)).ColorAndOpacity(FSlateColor(Accent))
            .Text_Lambda([HasSelection, Entry]()
            {
                if (!HasSelection()) { return FText::GetEmpty(); }
                const FTrainingInfo& Info = GetTrainingInfo(Entry());
                return FText::FromString(Info.bAvailable ? Info.Category : TEXT("PLANNED"));
            })
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 13)
        [
            SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 27)).ColorAndOpacity(FSlateColor(Ink))
            .AutoWrapText(true).Text_Lambda([HasSelection, Entry]()
            {
                return FText::FromString(HasSelection() ? GetTrainingInfo(Entry()).Name : TEXT("No matching skills"));
            })
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 20)
        [
            SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FSlateColor(Ink)).Text_Lambda([HasSelection, Entry]()
            {
                return HasSelection() ? FText::FromString(GetTrainingInfo(Entry()).Description) : FText::GetEmpty();
            })
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
        [
            SNew(SHorizontalBox)
            .Visibility_Lambda([IsKnowledge]() { return IsKnowledge() ? EVisibility::Visible : EVisibility::Collapsed; })
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SButton).ButtonStyle(&MenuButtonStyle()).ButtonColorAndOpacity(Tile).Text(FText::FromString(TEXT("−")))
                .IsEnabled_Lambda([this]() { return PreviewRank > 1; })
                .OnClicked_Lambda([this]() { --PreviewRank; return FReply::Handled(); })
            ]
            + SHorizontalBox::Slot().FillWidth(1).HAlign(HAlign_Center).VAlign(VAlign_Center)
            [
                SNew(STextBlock).Text_Lambda([this, IsKnowledge, Entry]()
                {
                    return IsKnowledge() ? FText::FromString(FString::Printf(TEXT("Rank %d / %d"), PreviewRank,
                        GetTrainingInfo(Entry()).MaxRank)) : FText::GetEmpty();
                })
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SButton).ButtonStyle(&MenuButtonStyle()).ButtonColorAndOpacity(Tile).Text(FText::FromString(TEXT("+")))
                .IsEnabled_Lambda([this, IsKnowledge, Entry]() { return IsKnowledge() && PreviewRank < GetTrainingInfo(Entry()).MaxRank; })
                .OnClicked_Lambda([this]() { ++PreviewRank; return FReply::Handled(); })
            ]
        ]
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FSlateColor(Muted))
            .Text(this, &SSurvivalInventoryMenu::TrainingRequirements)
        ]
        + SVerticalBox::Slot().FillHeight(1)[SNew(SSpacer)]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
        [
            SNew(SButton).ButtonStyle(&MenuButtonStyle()).ButtonColorAndOpacity(Chosen).HAlign(HAlign_Center)
            .Visibility_Lambda([IsKnowledge, Entry]() { return IsKnowledge() && GetTrainingInfo(Entry()).bAvailable ? EVisibility::Visible : EVisibility::Collapsed; })
            .IsEnabled_Lambda([this, IsKnowledge, Entry]() { return Player.IsValid() && IsKnowledge() && Player->GetProgression().CanBuyThrough(Entry(), PreviewRank); })
            .Text(FText::FromString(TEXT("Spend skill points")))
            .OnClicked_Lambda([this, Entry]()
            {
                if (ASurvivalCharacter* Character = Player.Get()) { Character->BuyTraining(Entry(), PreviewRank); }
                return FReply::Handled();
            })
        ]
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SButton).ButtonStyle(&MenuButtonStyle()).ButtonColorAndOpacity(Tile).HAlign(HAlign_Center)
            .Visibility_Lambda([HasSelection, Entry]()
            {
                return HasSelection() && ASurvivalCharacter::TrainingBook(Entry()) != ESurvivalItem::None
                    ? EVisibility::Visible : EVisibility::Collapsed;
            })
            .ToolTipText(FText::FromString(TEXT("Read a carried copy to gain one rank immediately.")))
            .IsEnabled_Lambda([this, HasSelection, Entry]()
            {
                return Player.IsValid() && HasSelection() && Player->CanUseItem(ASurvivalCharacter::TrainingBook(Entry()));
            })
            .Text_Lambda([this, HasSelection, Entry]()
            {
                if (!Player.IsValid() || !HasSelection()) { return FText::GetEmpty(); }
                const ESurvivalItem Book = ASurvivalCharacter::TrainingBook(Entry());
                return FText::FromString(FString::Printf(TEXT("Read %s  (%d held)"), SurvivalItemName(Book), Player->GetItemCount(Book)));
            })
            .OnClicked_Lambda([this, Entry]()
            {
                if (ASurvivalCharacter* Character = Player.Get()) { Character->UseItem(ASurvivalCharacter::TrainingBook(Entry())); }
                return FReply::Handled();
            })
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
            SNew(SButton).ButtonStyle(&MenuButtonStyle()).HAlign(HAlign_Center).Text(FText::FromString(Label))
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
            SNew(SButton).ButtonStyle(&MenuButtonStyle()).HAlign(HAlign_Fill).ButtonColorAndOpacity_Lambda([this, Index]()
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
            SNew(SBox).HeightOverride(335.0f)[Rows]
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
    const FSurvivalRecipe& Recipe = Recipes[SelectedRecipe];
    if (Recipe.RequiredTraining != ETraining::Count && Player.IsValid()
        && Player->GetProgression().Rank(Recipe.RequiredTraining) == 0)
    {
        Ingredients = FString::Printf(TEXT("Locked — learn %s in Skills or read its schematic.\n\n"),
            GetTrainingInfo(Recipe.RequiredTraining).Name);
    }
    for (const FSurvivalIngredient& Ingredient : Recipe.Ingredients)
    {
        const int32 Held = Player.IsValid() ? Player->GetItemCount(Ingredient.Item) : 0;
        Ingredients += FString::Printf(TEXT("%s   %d / %d\n"), SurvivalItemName(Ingredient.Item), Held, Ingredient.Count * Quantity);
    }
    return FText::FromString(Ingredients.TrimEnd());
}

TSharedRef<SWidget> SSurvivalInventoryMenu::MakeRecipeDetails()
{
    return SNew(SBorder).Padding(FMargin(20.0f))
        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(PanelColor)
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
                    SNew(SButton).ButtonStyle(&MenuButtonStyle()).ButtonColorAndOpacity(Chosen).HAlign(HAlign_Center).Text(FText::FromString(TEXT("Craft")))
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
            if (Page == EPage::Pack && Event.GetKey() == SlotKeys[Slot])
            {
                Character->ChooseHotbarSlot(Slot);
                return FReply::Handled();
            }
        }
    }
    return SCompoundWidget::OnKeyDown(Geometry, Event);
}
