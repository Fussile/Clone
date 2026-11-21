#include "SurvivalInventoryMenu.h"

#include "Engine/Texture2D.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "SurvivalCharacter.h"
#include "SurvivalHUD.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
const FLinearColor Accent(0.35f, 0.85f, 0.84f);
const FLinearColor Backdrop(0.025f, 0.050f, 0.065f, 0.98f);
const FLinearColor Tile(0.17f, 0.25f, 0.29f);
const FLinearColor Chosen(0.24f, 0.55f, 0.53f);
const FKey SlotKeys[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six};
static_assert(UE_ARRAY_COUNT(SlotKeys) == ASurvivalCharacter::HotbarSize);
}

const FSlateBrush* SSurvivalInventoryMenu::FindIcon(ESurvivalItem Item) const
{
    return Item != ESurvivalItem::None ? &Icons[static_cast<int32>(Item)] : nullptr;
}

void SSurvivalInventoryMenu::Construct(const FArguments& Args)
{
    Player = Args._Player;
    TSharedRef<SUniformGridPanel> Items = SNew(SUniformGridPanel).SlotPadding(FMargin(3.0f));
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Icons); ++Index)
    {
        const ESurvivalItem Item = static_cast<ESurvivalItem>(Index);
        Icons[Index].SetImageSize(FVector2D(36.0, 36.0));
        Icons[Index].SetResourceObject(Args._HUD->GetItemIcon(Item));
        Icons[Index].DrawAs = Icons[Index].GetResourceObject() ? ESlateBrushDrawType::Image : ESlateBrushDrawType::NoDrawType;
        Items->AddSlot(Index % 4, Index / 4)
        [
            SNew(SButton)
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
            SNew(SButton)
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
    .HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(14.0f)
    [
        SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
        [
            SNew(SBox).WidthOverride(720.0f)
            [
                SNew(SBorder).Padding(FMargin(18.0f))
                .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Backdrop)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                        [
                            SNew(STextBlock).Text(FText::FromString(TEXT("Pack & Craft")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 22)).ColorAndOpacity(FSlateColor(Accent))
                        ]
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(SButton).Text(FText::FromString(TEXT("Close [I]")))
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
                        + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 12, 0)
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight()[Items]
                            + SVerticalBox::Slot().AutoHeight().Padding(3, 9, 3, 0)
                            [
                                SNew(SButton).HAlign(HAlign_Center)
                                .Text_Lambda([this]()
                                {
                                    const ESurvivalItem Item = Player.IsValid()
                                        ? Player->GetSelectedInventoryItem() : ESurvivalItem::None;
                                    return FText::FromString(FString::Printf(TEXT("Use %s"), SurvivalItemName(Item)));
                                })
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
                        ]
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(SBox).WidthOverride(184.0f)
                            [
                                SNew(SVerticalBox)
                                + SVerticalBox::Slot().AutoHeight().Padding(4, 4, 4, 8)
                                [
                                    SNew(STextBlock).Text(FText::FromString(TEXT("Crafting")))
                                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14)).ColorAndOpacity(FSlateColor(Accent))
                                ]
                                + SVerticalBox::Slot().AutoHeight().Padding(4)
                                [
                                    SNew(SImage).Image(FindIcon(ESurvivalItem::Bandage))
                                ]
                                + SVerticalBox::Slot().AutoHeight().Padding(4)
                                [
                                    SNew(STextBlock).Text_Lambda([this]()
                                    {
                                        const int32 Cloth = Player.IsValid() ? Player->GetItemCount(ESurvivalItem::Cloth) : 0;
                                        return FText::FromString(FString::Printf(TEXT("Bandage\n%d cloth needed (%d held)"),
                                            FSurvivalItems::BandageCloth, Cloth));
                                    })
                                ]
                                + SVerticalBox::Slot().AutoHeight().Padding(4, 8)
                                [
                                    SNew(SButton).HAlign(HAlign_Center).Text(FText::FromString(TEXT("Craft bandage")))
                                    .IsEnabled_Lambda([this]()
                                    {
                                        return Player.IsValid()
                                            && Player->GetItemCount(ESurvivalItem::Cloth) >= FSurvivalItems::BandageCloth;
                                    })
                                    .OnClicked_Lambda([this]()
                                    {
                                        if (ASurvivalCharacter* Character = Player.Get())
                                        {
                                            Character->CraftBandage();
                                        }
                                        return FReply::Handled();
                                    })
                                ]
                            ]
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(3, 16, 3, 5)
                    [
                        SNew(STextBlock).Text(FText::FromString(TEXT("Select an item, then click a slot or press 1-6 to assign it.")))
                        .ColorAndOpacity(FSlateColor(Accent))
                    ]
                    + SVerticalBox::Slot().AutoHeight()[Slots]
                ]
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
