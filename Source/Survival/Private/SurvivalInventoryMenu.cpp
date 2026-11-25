#include "SurvivalInventoryMenu.h"

#include "Brushes/SlateColorBrush.h"
#include "Engine/Texture2D.h"
#include "Input/Events.h"
#include "Input/DragAndDrop.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SWrapBox.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "SurvivalCharacter.h"
#include "SurvivalHUD.h"
#include "SurvivalGear.h"
#include "SurvivalWeaponMods.h"
#include "Engine/TextureRenderTarget2D.h"
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
const FLinearColor Backdrop(0.019f, 0.024f, 0.027f, 0.30f);
const FLinearColor PanelColor(0.043f, 0.050f, 0.054f, 0.64f);
const FLinearColor Tile(0.081f, 0.093f, 0.101f, 0.60f);
const FLinearColor Chosen(0.46f, 0.23f, 0.095f);

struct FTrainingTree { const TCHAR* Label; const TCHAR* Category; const TCHAR* Icon; };
constexpr FTrainingTree TrainingTrees[] = {
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

// Quality tiers for part bars and tooltips: grey, green, blue, purple, gold.
FLinearColor QualityColor(int32 Quality)
{
    if (Quality >= 800) { return FLinearColor(0.84f, 0.67f, 0.25f); }
    if (Quality >= 650) { return FLinearColor(0.55f, 0.42f, 0.72f); }
    if (Quality >= 500) { return FLinearColor(0.30f, 0.52f, 0.72f); }
    if (Quality >= 350) { return FLinearColor(0.36f, 0.60f, 0.34f); }
    return FLinearColor(0.55f, 0.56f, 0.55f);
}

ESurvivalItem AssemblyWeapon(ESurvivalItem Item)
{
    return Item == ESurvivalItem::Sidearm || FSurvivalWeaponAssembly::PartIndex(Item, ESurvivalItem::Sidearm) != INDEX_NONE
        ? ESurvivalItem::Sidearm : ESurvivalItem::Rifle;
}

bool IsModifiable(ESurvivalItem Item)
{
    return Item == ESurvivalItem::Sidearm || Item == ESurvivalItem::Rifle || SurvivalItemHasQuality(Item);
}

int32 CountSpareParts(const ASurvivalCharacter* Character, ESurvivalItem Weapon)
{
    int32 Count = 0;
    for (int32 Index = 0; Character && Index < Character->GetPackSlotCount(); ++Index)
    {
        Count += FSurvivalWeaponAssembly::PartIndex(Character->GetPackItem(Index), Weapon) != INDEX_NONE ? 1 : 0;
    }
    return Count;
}

FText DescribeItem(const ASurvivalCharacter* Character, ESurvivalItem Item, int32 Quality, int32 StackCount)
{
    if (!Character || Item == ESurvivalItem::None)
    {
        return FText::GetEmpty();
    }
    const int32 Carried = Character->GetItemCount(Item);
    FString Text = SurvivalItemName(Item);
    const auto Weapon = AssemblyWeapon(Item);
    if (const int32 Slot = FSurvivalWeaponAssembly::PartIndex(Item, Weapon); Slot != INDEX_NONE)
    {
        Text += FString::Printf(TEXT("   quality %d"), Quality);
        if (Character->GetItemCount(Weapon) > 0)
        {
            const int32 Installed = Character->GetWeaponAssembly(Weapon).GetPartQuality(Slot);
            Text += FString::Printf(TEXT("\nInstalled %d   (%+d if swapped in)"), Installed, Quality - Installed);
        }
        Text += TEXT("\nOpen the matching weapon in Modify, then drag this onto its slot");
        return FText::FromString(Text);
    }
    Text += StackCount >= 0 && StackCount != Carried
        ? FString::Printf(TEXT("   x%d   (%d carried)"), StackCount, Carried)
        : FString::Printf(TEXT("   x%d"), Carried);
    if (Item == ESurvivalItem::Rifle || Item == ESurvivalItem::Sidearm)
        Text += FString::Printf(TEXT("\nQuality %d\nSelect it and choose Modify to swap parts"), Quality);
    if (Character->CanUseItem(Item))
    {
        Text += TEXT("\nRight-click to use");
    }
    return FText::FromString(Text);
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

class FPackDrag : public FDragDropOperation
{
public:
    DRAG_DROP_OPERATOR_TYPE(FPackDrag, FDragDropOperation)
    bool bBelt;
    int32 Slot;
    bool bHalf;
    int32 GearIndex = INDEX_NONE;
    bool bLoot = false;
    int32 LootRevision = 0;
    FText Label;
    FSlateBrush Icon;

    virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
    {
        return SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(PanelColor).Padding(8)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()[SNew(SImage).Image(&Icon)]
            + SHorizontalBox::Slot().AutoWidth().Padding(6).VAlign(VAlign_Center)[SNew(STextBlock).Text(Label)]
        ];
    }

    static TSharedRef<FPackDrag> Start(bool bIsBelt, int32 SourceSlot, ESurvivalItem Item, const FSlateBrush* SourceIcon, bool bSplit, int32 Gear = INDEX_NONE, bool bFromLoot = false, int32 Revision = 0)
    {
        auto Op = MakeShared<FPackDrag>();
        Op->bBelt = bIsBelt;
        Op->Slot = SourceSlot;
        Op->bHalf = bSplit;
        Op->GearIndex = Gear;
        Op->bLoot = bFromLoot;
        Op->LootRevision = Revision;
        Op->Label = FText::FromString(Gear == INDEX_NONE ? SurvivalItemName(Item) : SurvivalGear[Gear].Label);
        if (SourceIcon) { Op->Icon = *SourceIcon; }
        Op->Construct();
        return Op;
    }
};

class SPortraitOrbit : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPortraitOrbit) {}
        SLATE_ARGUMENT(ASurvivalCharacter*, Player)
        SLATE_ARGUMENT(const FSlateBrush*, Brush)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args)
    {
        Player = Args._Player;
        ChildSlot[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image(Args._Brush)]];
        SetToolTipText(FText::FromString(TEXT("Drag to rotate")));
    }

    FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent& Event) override
    {
        return Event.GetEffectingButton() == EKeys::LeftMouseButton
            ? FReply::Handled().CaptureMouse(SharedThis(this)) : FReply::Unhandled();
    }

    FReply OnMouseMove(const FGeometry&, const FPointerEvent& Event) override
    {
        if (!HasMouseCapture() || !Player.IsValid()) return FReply::Unhandled();
        Player->RotateCharacterPortrait(static_cast<float>(Event.GetCursorDelta().X * 0.75));
        return FReply::Handled();
    }

    FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent& Event) override
    {
        return HasMouseCapture() && Event.GetEffectingButton() == EKeys::LeftMouseButton
            ? FReply::Handled().ReleaseMouseCapture() : FReply::Unhandled();
    }
private:
    TWeakObjectPtr<ASurvivalCharacter> Player;
};

class SWearSlot : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWearSlot) {}
        SLATE_ARGUMENT(ASurvivalCharacter*, Player)
        SLATE_ARGUMENT(FString, WearSlot)
        SLATE_ARGUMENT(const FSlateBrush*, GearIcons)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args)
    {
        Player = Args._Player;
        GearIcons = Args._GearIcons;
        Slot = Args._WearSlot;
        ChildSlot[SNew(SButton).ButtonStyle(&MenuButtonStyle()).ButtonColorAndOpacity(Tile)
            .ToolTipText(FText::FromString(TEXT("Drag matching gear here. Click a worn item to return it to the backpack.")))
            .OnClicked_Lambda([this]{ const int32 Id = Worn(); if (Id != INDEX_NONE) Player->ServerSetGearEquipped(Id,false); return FReply::Handled(); })
            [SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [SNew(SImage).Image_Lambda([this]{ const int32 Id = Worn(); return Id != INDEX_NONE && GearIcons[Id].GetResourceObject() ? GearIcons + Id : nullptr; })]
                + SHorizontalBox::Slot().FillWidth(1).Padding(4,0).VAlign(VAlign_Center)
                [SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]{
                const int32 Id = Worn();
                return FText::FromString(Slot + TEXT("\n") + (Id == INDEX_NONE ? TEXT("Empty") : SurvivalGear[Id].Label));
            })]]];
    }
    virtual FReply OnDrop(const FGeometry&, const FDragDropEvent& Event) override
    {
        const auto Op = Event.GetOperationAs<FPackDrag>();
        if (!Op || Op->bLoot || !Player.IsValid() || Op->GearIndex == INDEX_NONE ||
            Player->GetPackCell(Op->Slot).GearIndex != Op->GearIndex || Slot != SurvivalGear[Op->GearIndex].Slot)
            return FReply::Unhandled();
        Player->ServerSetGearEquipped(Op->GearIndex,true);
        return FReply::Handled();
    }
private:
    int32 Worn() const
    {
        if (Player.IsValid())
            for (int32 Id = 0; Id < UE_ARRAY_COUNT(SurvivalGear); ++Id)
                if (Slot == SurvivalGear[Id].Slot && Player->IsWearing(Id)) return Id;
        return INDEX_NONE;
    }
    const FSlateBrush* GearIcons = nullptr;
    TWeakObjectPtr<ASurvivalCharacter> Player;
    FString Slot;
};

class SPackCell : public SCompoundWidget
{
public:
    virtual bool SupportsKeyboardFocus() const override { return true; }
    SLATE_BEGIN_ARGS(SPackCell) {}
        SLATE_ARGUMENT(ASurvivalCharacter*, Player)
        SLATE_ARGUMENT(bool, Belt)
        SLATE_ARGUMENT(int32, Index)
        SLATE_ARGUMENT(const FSlateBrush*, Icons)
        SLATE_ARGUMENT(const FSlateBrush*, GearIcons)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args)
    {
        Player = Args._Player;
        GearIcons = Args._GearIcons;
        bBelt = Args._Belt;
        Index = Args._Index;
        Icons = Args._Icons;
        SetToolTipText(TAttribute<FText>::CreateLambda([this]() {
            const FSurvivalPackCell Cell = bBelt || !Player.IsValid() ? FSurvivalPackCell() : Player->GetPackCell(Index);
            if (Cell.GearIndex != INDEX_NONE) return FText::FromString(FString::Printf(TEXT("%s  ·  %s\nShift-click to equip"), SurvivalGear[Cell.GearIndex].Label, SurvivalGear[Cell.GearIndex].Slot));
            const int32 Count = bBelt && Player.IsValid() ? Player->GetToolbeltCount(Index) : Cell.Count;
            return DescribeItem(Player.Get(), Item(), HasQuality() ? Quality() : Cell.Quality, Count);
        }));
        ChildSlot
        [
            SNew(SBorder).BorderImage(WhiteBrush()).Padding(2)
            .BorderBackgroundColor_Lambda([this]() {
                const bool bSelected = Player.IsValid() && (bBelt ? Index == Player->GetActiveHotbarSlot()
                    : (Item() != ESurvivalItem::None || Gear() != INDEX_NONE) && Index == Player->GetSelectedPackSlot());
                return FSlateColor(IsHovered() ? Accent : bSelected ? Chosen : PanelColor);
            })
            [
                SNew(SBorder).Padding(4).BorderImage(WhiteBrush()).BorderBackgroundColor(Tile)
                [
                    SNew(SOverlay)
                    + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
                    [SNew(SImage).DesiredSizeOverride(FVector2D(52,52)).Image_Lambda([this]() { return Icon(); })]
                    + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
                    [SNew(STextBlock).WrapTextAt(128).Justification(ETextJustify::Center)
                        .Text_Lambda([this]{
                            const int32 Id = Gear();
                            if (Id != INDEX_NONE) return GearIcons[Id].GetResourceObject() ? FText::GetEmpty() : FText::FromString(SurvivalGear[Id].Label);
                            const auto Value = Item();
                            return Value != ESurvivalItem::None && !Icons[static_cast<int32>(Value)].GetResourceObject()
                                ? FText::FromString(SurvivalItemName(Value)) : FText::GetEmpty();
                        })]
                    + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
                    [SNew(STextBlock).ColorAndOpacity(Muted).Text(bBelt ? FText::AsNumber(Index + 1) : FText::GetEmpty())]
                    + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom)
                    [
                        SNew(STextBlock).ShadowOffset(FVector2D(1,1))
                        .ColorAndOpacity_Lambda([this]() {
                            return FSlateColor(HasQuality() ? QualityColor(Quality()) : FLinearColor::White);
                        })
                        .Text_Lambda([this]() { return CountText(); })
                    ]
                ]
            ]
        ];
    }

    virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent& Event) override
    {
        if (Event.GetEffectingButton() == EKeys::RightMouseButton)
        {
            if (!Player.IsValid() || Item() == ESurvivalItem::None) { return FReply::Unhandled(); }
            if (bBelt) { Player->SelectToolbeltItem(Index); }
            else { Player->SetSelectedPackSlot(Index); }
            if (bBelt) Player->UseToolbeltSlot(Index);
            else Player->UseItem(Item());
            return FReply::Handled();
        }
        if (Event.GetEffectingButton() != EKeys::LeftMouseButton) { return FReply::Unhandled(); }
        return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse).CaptureMouse(SharedThis(this)).DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
    }

    virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override
    {
        if (!HasMouseCapture() || Event.GetEffectingButton() != EKeys::LeftMouseButton) { return FReply::Unhandled(); }
        if (Player.IsValid() && Geometry.IsUnderLocation(Event.GetScreenSpacePosition()))
        {
            if (bBelt) { Player->SelectToolbeltItem(Index); Player->ChooseHotbarSlot(Index); }
            else if (Gear() != INDEX_NONE)
            {
                Player->SetSelectedPackSlot(Index);
                if (Event.IsShiftDown()) Player->ServerSetGearEquipped(Gear(), true);
            }
            else if (Item() == ESurvivalItem::None) { Player->SetSelectedInventoryItem(ESurvivalItem::None); }
            else if (Event.IsShiftDown()) { QuickAssign(); }
            else { Player->SetSelectedPackSlot(Index); }
        }
        return FReply::Handled().ReleaseMouseCapture();
    }

    virtual FReply OnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event) override
    {
        if (Event.GetEffectingButton() == EKeys::LeftMouseButton && !bBelt && Item() != ESurvivalItem::None)
        {
            QuickAssign();
            return FReply::Handled();
        }
        return OnMouseButtonDown(Geometry, Event);
    }

    virtual FReply OnDragDetected(const FGeometry&, const FPointerEvent& Event) override
    {
        if (Item() == ESurvivalItem::None && Gear() == INDEX_NONE) { return FReply::Unhandled(); }
        return FReply::Handled().ReleaseMouseCapture().BeginDragDrop(
            FPackDrag::Start(bBelt, Index, Item(), Icon(), Event.IsControlDown(), Gear()));
    }

    virtual FReply OnDrop(const FGeometry&, const FDragDropEvent& Event) override
    {
        const auto Op = Event.GetOperationAs<FPackDrag>();
        if (!Player.IsValid() || !Op) { return FReply::Unhandled(); }
        if (Op->bLoot)
        {
            if (bBelt || Op->LootRevision != Player->GetContainerRevision()) return FReply::Unhandled();
            Player->TakeContainerLoot(Op->Slot, Index);
            return FReply::Handled();
        }
        const bool bMoved = !Op->bBelt && !bBelt
            ? Player->MovePackStack(Op->Slot, Index, Op->bHalf)
            : Player->MoveMenuItem(Op->bBelt, Op->Slot, bBelt, Index);
        return bMoved ? FReply::Handled() : FReply::Unhandled();
    }

private:
    // Double-click or Shift-click: reuse the slot already holding the item, else the first empty slot, else the active one.
    void QuickAssign()
    {
        if (!Player.IsValid() || Item() == ESurvivalItem::None || SurvivalItemHasQuality(Item())) { return; }
        const ESurvivalItem Value = Item();
        if (!ASurvivalCharacter::CanQuickSlot(Value)) return;
        int32 Target = Player->GetActiveHotbarSlot();
        for (int32 Slot = 0; Slot < ASurvivalCharacter::HotbarSize; ++Slot)
        {
            if (Player->GetHotbarItem(Slot) == ESurvivalItem::None)
            {
                Target = Slot;
                break;
            }
        }
        Player->MoveMenuItem(false, Index, true, Target);
    }

    int32 Gear() const { return !bBelt && Player.IsValid() ? Player->GetPackCell(Index).GearIndex : INDEX_NONE; }
    ESurvivalItem Item() const
    {
        if (!Player.IsValid()) { return ESurvivalItem::None; }
        return bBelt ? Player->GetHotbarItem(Index) : Player->GetPackItem(Index);
    }
    bool HasQuality() const { return SurvivalItemHasQuality(Item()) || Item() == ESurvivalItem::Sidearm || Item() == ESurvivalItem::Rifle; }
    int32 Quality() const
    {
        if (!Player.IsValid()) return 0;
        const auto Cell = bBelt ? FSurvivalPackCell{} : Player->GetPackCell(Index);
        return SurvivalItemHasQuality(Item()) ? Cell.Quality
            : Player->GetWeaponQuality(bBelt ? Player->GetToolbeltWeaponId(Index) : Cell.WeaponId);
    }
    FText CountText() const
    {
        if (Gear() != INDEX_NONE)
        {
            const int32 Count = Player->GetPackCell(Index).Count;
            return Count > 1 ? FText::AsNumber(Count) : FText::GetEmpty();
        }
        const ESurvivalItem Value = Item();
        if (Value == ESurvivalItem::None) { return FText::GetEmpty(); }
        if (SurvivalItemHasQuality(Value)) { return FText::AsNumber(Quality()); }
        if (bBelt) { return FText::AsNumber(Player->GetToolbeltCount(Index)); }
        const int32 Amount = Player->GetPackCell(Index).Count;
        return Amount > 1 ? FText::AsNumber(Amount) : FText::GetEmpty();
    }
    const FSlateBrush* Icon() const
    {
        const int32 Id = Gear();
        if (Id != INDEX_NONE) return GearIcons[Id].GetResourceObject() ? GearIcons + Id : nullptr;
        const auto Value = Item();
        return Value == ESurvivalItem::None ? nullptr : Icons + static_cast<int32>(Value);
    }

    const FSlateBrush* GearIcons = nullptr;
    TWeakObjectPtr<ASurvivalCharacter> Player;
    const FSlateBrush* Icons = nullptr;
    bool bBelt = false;
    int32 Index = 0;
};

// A rifle part slot in the Modify panel. Drop a matching spare part on it to swap it in.
class SPartSlot : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPartSlot) {}
        SLATE_ARGUMENT(ASurvivalCharacter*, Player)
        SLATE_ARGUMENT(int32, PartSlot)
        SLATE_ARGUMENT(const FSlateBrush*, Icons)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args)
    {
        Player = Args._Player;
        Slot = Args._PartSlot;
        Icons = Args._Icons;
        ChildSlot
        [
            SNew(SBorder).BorderImage(WhiteBrush()).Padding(2)
            .BorderBackgroundColor_Lambda([this]() { return FSlateColor(bDragOver ? Accent : PanelColor); })
            [
                SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(Tile).Padding(8)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
                        [SNew(SImage).DesiredSizeOverride(FVector2D(34, 34)).Image_Lambda([this]{
                            return Icons + static_cast<int32>(FSurvivalWeaponAssembly::FirstPart(Weapon())) + Slot;
                        })]
                        + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                        [
                            SNew(STextBlock).ColorAndOpacity(FSlateColor(Ink))
                            .Text_Lambda([this]{ return FText::FromString(FSurvivalWeaponAssembly::PartName(Slot, Weapon())); })
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        [
                            SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
                            .ColorAndOpacity_Lambda([this]() { return FSlateColor(QualityColor(Installed())); })
                            .Text_Lambda([this]() { return Installed() > 0 ? FText::AsNumber(Installed()) :
                                FText::FromString(TEXT("Missing")); })
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
                    [
                        SNew(SButton).Text(FText::FromString(TEXT("Remove to backpack")))
                        .Visibility_Lambda([this]{ return Installed() > 0 ? EVisibility::Visible : EVisibility::Collapsed; })
                        .IsEnabled_Lambda([this]{ return Player.IsValid() && Player->CanInspectWeapon(Weapon()); })
                        .OnClicked_Lambda([this]{ if (Player.IsValid()) Player->RemoveWeaponPart(Slot); return FReply::Handled(); })
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
                    [
                        SNew(SBox).HeightOverride(5)
                        [
                            SNew(SProgressBar)
                            .FillColorAndOpacity_Lambda([this]() { return FSlateColor(QualityColor(Installed())); })
                            .Percent_Lambda([this]() -> TOptional<float>
                            {
                                return FMath::Clamp(static_cast<float>(Installed()) / static_cast<float>(FSurvivalWeaponAssembly::MaxQuality), 0.0f, 1.0f);
                            })
                        ]
                    ]
                ]
            ]
        ];
    }

    virtual void OnDragEnter(const FGeometry&, const FDragDropEvent& Event) override { bDragOver = Accepts(Event); }
    virtual void OnDragLeave(const FDragDropEvent&) override { bDragOver = false; }

    virtual FReply OnDrop(const FGeometry&, const FDragDropEvent& Event) override
    {
        bDragOver = false;
        const auto Op = Event.GetOperationAs<FPackDrag>();
        return Op && Accepts(Event) && Player->SwapWeaponPart(Op->Slot) ? FReply::Handled() : FReply::Unhandled();
    }

private:
    bool Accepts(const FDragDropEvent& Event) const
    {
        const auto Op = Event.GetOperationAs<FPackDrag>();
        return Op && !Op->bLoot && !Op->bBelt && Player.IsValid() && Player->CanInspectWeapon(Weapon())
            && FSurvivalWeaponAssembly::PartIndex(Player->GetPackItem(Op->Slot), Weapon()) == Slot;
    }
    ESurvivalItem Weapon() const { return Player.IsValid() ? Player->InspectedWeapon : ESurvivalItem::None; }
    int32 Installed() const { return Player.IsValid() ? Player->GetWeaponAssembly(Weapon()).GetPartQuality(Slot) : 0; }
    const FSlateBrush* Icons = nullptr;

    TWeakObjectPtr<ASurvivalCharacter> Player;
    int32 Slot = 0;
    bool bDragOver = false;
};

// One spare part from the pack, listed in the Modify panel. Drag it onto the matching slot.
class SSpareRow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SSpareRow) {}
        SLATE_ARGUMENT(ASurvivalCharacter*, Player)
        SLATE_ARGUMENT(int32, PackSlot)
        SLATE_ARGUMENT(const FSlateBrush*, Icons)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args)
    {
        Player = Args._Player;
        PackSlot = Args._PackSlot;
        Icons = Args._Icons;
        SetToolTipText(FText::FromString(TEXT("Drag onto the matching slot above to swap it in.")));
        ChildSlot
        [
            SNew(SBox).Visibility_Lambda([this]() { return CellIndex() != INDEX_NONE ? EVisibility::Visible : EVisibility::Collapsed; })
            [
                SNew(SBorder).BorderImage(WhiteBrush()).Padding(2)
                .BorderBackgroundColor_Lambda([this]() { return FSlateColor(IsHovered() ? Accent : PanelColor); })
                [
                    SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(Tile).Padding(6)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
                        [SNew(SImage).DesiredSizeOverride(FVector2D(30, 30)).Image_Lambda([this]() { return PartIcon(); })]
                        + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                        [
                            SNew(STextBlock).ColorAndOpacity(FSlateColor(Ink)).Text_Lambda([this]()
                            {
                                return FText::FromString(SurvivalItemName(Cell().Item));
                            })
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
                        [
                            SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
                            .ColorAndOpacity_Lambda([this]() { return FSlateColor(QualityColor(Cell().Quality)); })
                            .Text_Lambda([this]() { return FText::AsNumber(Cell().Quality); })
                        ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                            .ColorAndOpacity_Lambda([this]()
                            {
                                const int32 Change = Delta();
                                return FSlateColor(Change > 0 ? FLinearColor(0.42f, 0.78f, 0.38f)
                                    : Change < 0 ? FLinearColor(0.88f, 0.40f, 0.32f) : Muted);
                            })
                            .Text_Lambda([this]() { return FText::FromString(FString::Printf(TEXT("%+d"), Delta())); })
                        ]
                    ]
                ]
            ]
        ];
    }

    virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent& Event) override
    {
        if (Event.GetEffectingButton() != EKeys::LeftMouseButton) { return FReply::Unhandled(); }
        return FReply::Handled().CaptureMouse(SharedThis(this)).DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
    }

    virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override
    {
        if (!HasMouseCapture() || Event.GetEffectingButton() != EKeys::LeftMouseButton) { return FReply::Unhandled(); }
        if (Player.IsValid() && Geometry.IsUnderLocation(Event.GetScreenSpacePosition()))
        {
            Player->SetSelectedPackSlot(CellIndex());
        }
        return FReply::Handled().ReleaseMouseCapture();
    }

    virtual FReply OnDragDetected(const FGeometry&, const FPointerEvent&) override
    {
        const int32 Index = CellIndex();
        if (Index == INDEX_NONE) { return FReply::Unhandled(); }
        return FReply::Handled().ReleaseMouseCapture().BeginDragDrop(
            FPackDrag::Start(false, Index, Cell().Item, PartIcon(), false));
    }

private:
    int32 CellIndex() const
    {
        return Player.IsValid() && FSurvivalWeaponAssembly::PartIndex(Player->GetPackItem(PackSlot), Player->InspectedWeapon) != INDEX_NONE
            ? PackSlot : INDEX_NONE;
    }
    FSurvivalPackCell Cell() const { return Player.IsValid() ? Player->GetPackCell(CellIndex()) : FSurvivalPackCell(); }
    const FSlateBrush* PartIcon() const
    {
        const ESurvivalItem Item = Cell().Item;
        return Item == ESurvivalItem::None ? nullptr : Icons + static_cast<int32>(Item);
    }
    int32 Delta() const
    {
        const FSurvivalPackCell Value = Cell();
        if (!Player.IsValid()) return 0;
        const auto Weapon = Player->InspectedWeapon;
        const int32 Slot = FSurvivalWeaponAssembly::PartIndex(Value.Item, Weapon);
        return Slot == INDEX_NONE ? 0 : Value.Quality - Player->GetWeaponAssembly(Weapon).GetPartQuality(Slot);
    }

    TWeakObjectPtr<ASurvivalCharacter> Player;
    const FSlateBrush* Icons = nullptr;
    int32 PackSlot = INDEX_NONE;
};

void LoadGearBrushes(TArray<FSlateBrush>& GearIcons, TStrongObjectPtr<UTexture2D>& AtlasTexture)
{
    GearIcons.SetNum(UE_ARRAY_COUNT(SurvivalGear));
    if (UTexture2D* Atlas = LoadObject<UTexture2D>(nullptr,
        TEXT("/Game/Art/Crossroads/Interface/T_UI_GearAtlas.T_UI_GearAtlas"), nullptr, LOAD_NoWarn))
    {
        AtlasTexture.Reset(Atlas);
        // The supplied sheet has uneven rows and labels under the artwork. Keep those labels outside the UVs.
        constexpr int32 Edges[][2] = {{4,143}, {167,302}, {325,451}, {496,638},
            {661,778}, {795,903}, {919,1009}, {1031,1142}};
        constexpr int32 Starts[][6] = {{0,153,331,635,800,800}, {0,153,328,603,800,800},
            {0,153,329,475,639,800}, {0,151,326,477,637,800}, {0,151,340,482,641,800},
            {0,152,322,480,639,800}, {0,155,327,478,637,800}, {0,153,327,476,635,800}};
        constexpr int32 Cells[] = { 0,1,3,10,11,12,13,20,21,22,23,-1,-1,-1,33,30,31,32,34,
            40,41,42,43,50,51,52,53,60,61,62,64,70,71,73,74 };
        static_assert(UE_ARRAY_COUNT(Cells) == UE_ARRAY_COUNT(SurvivalGear));
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Cells); ++Index)
        {
            if (Cells[Index] < 0) continue;
            const int32 Row = Cells[Index] / 10;
            const int32 Column = Cells[Index] % 10;
            FSlateBrush& Brush = GearIcons[Index];
            Brush.DrawAs = ESlateBrushDrawType::Image;
            Brush.SetResourceObject(Atlas);
            Brush.SetImageSize(FVector2D(44,44));
            Brush.SetUVRegion(FBox2f(
                FVector2f((Starts[Row][Column] + 2) / 800.0f, Edges[Row][0] / 1303.0f),
                FVector2f((Starts[Row][Column + 1] - 2) / 800.0f, Edges[Row][1] / 1303.0f)));
        }
    }
}

class SLootSlot : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SLootSlot) {}
        SLATE_ARGUMENT(ASurvivalCharacter*, Character)
        SLATE_ARGUMENT(int32, Index)
        SLATE_ARGUMENT(const FSlateBrush*, Icons)
        SLATE_ARGUMENT(const FSlateBrush*, GearIcons)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args)
    {
        Player = Args._Character;
        Index = Args._Index;
        Icons = Args._Icons;
        GearIcons = Args._GearIcons;
        SetToolTipText(TAttribute<FText>::CreateLambda([this]{
            const auto* Slot = Entry();
            if (!Slot || Slot->IsEmpty()) return FText::GetEmpty();
            if (Slot->Quality > 0)
                return FText::FromString(FString::Printf(TEXT("%s · quality %d"), Name(*Slot), Slot->Quality));
            return FText::FromString(FString::Printf(TEXT("%s ×%d"), Name(*Slot), Slot->Count));
        }));

        ChildSlot
        [
            SNew(SBox).WidthOverride(50).HeightOverride(50)
            [
                SNew(SBorder).BorderImage(WhiteBrush()).Padding(3)
                .BorderBackgroundColor_Lambda([this]{ return IsHovered() ? Accent : Tile; })
                [
                    SNew(SOverlay)
                    + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
                    [SNew(SImage).DesiredSizeOverride(FVector2D(38,38)).Image_Lambda([this]{ return Icon(); })]
                    + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
                    [
                        SNew(SImage).Image(WhiteBrush()).DesiredSizeOverride(FVector2D(12,12)).ColorAndOpacity(Muted)
                        .Visibility_Lambda([this]{
                            const auto* Slot = Entry();
                            return Slot && !Slot->IsEmpty() && !Icon() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
                        })
                    ]
                    + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom)
                    [
                        SNew(STextBlock).ShadowOffset(FVector2D(1,1)).Text_Lambda([this]{
                            const auto* Slot = Entry();
                            return Slot && Slot->Count > 1 ? FText::AsNumber(Slot->Count) : FText::GetEmpty();
                        })
                    ]
                ]
            ]
        ];
    }

    FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent& Event) override
    {
        const auto* Slot = Entry();
        if (Event.GetEffectingButton() != EKeys::LeftMouseButton || !Slot || Slot->IsEmpty()) return FReply::Unhandled();
        if (Event.IsShiftDown())
        {
            Player->TakeContainerLoot(Index);
            return FReply::Handled();
        }
        return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
    }

    FReply OnDragDetected(const FGeometry&, const FPointerEvent& Event) override
    {
        const auto* Slot = Entry();
        if (!Slot || Slot->IsEmpty()) return FReply::Unhandled();
        return FReply::Handled().BeginDragDrop(FPackDrag::Start(false, Index, Slot->Item(), Icon(),
            Event.IsControlDown(), Slot->GearIndex, true, Player->GetContainerRevision()));
    }

    FReply OnDrop(const FGeometry&, const FDragDropEvent& Event) override
    {
        const auto Op = Event.GetOperationAs<FPackDrag>();
        if (!Player.IsValid() || !Op || !Op->bLoot || Op->LootRevision != Player->GetContainerRevision())
            return FReply::Unhandled();
        Player->MoveContainerLoot(Op->Slot, Index, Op->bHalf);
        return FReply::Handled();
    }

private:
    const FSurvivalLootSlot* Entry() const { return Player.IsValid() ? Player->GetContainerSlot(Index) : nullptr; }

    static const TCHAR* Name(const FSurvivalLootSlot& Slot)
    {
        return Slot.GearIndex != INDEX_NONE ? SurvivalGear[Slot.GearIndex].Label : SurvivalItemName(Slot.Item());
    }

    const FSlateBrush* Icon() const
    {
        const FSurvivalLootSlot* Slot = Entry();
        if (!Slot || Slot->IsEmpty()) return nullptr;
        const FSlateBrush& Brush = Slot->GearIndex != INDEX_NONE ? GearIcons[Slot->GearIndex] : Icons[Slot->ItemId];
        return Brush.GetResourceObject() ? &Brush : nullptr;
    }

    TWeakObjectPtr<ASurvivalCharacter> Player;
    const FSlateBrush* Icons = nullptr;
    const FSlateBrush* GearIcons = nullptr;
    int32 Index = INDEX_NONE;
};

class SLootGrid : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SLootGrid) {}
        SLATE_ARGUMENT(ASurvivalCharacter*, Character)
        SLATE_ARGUMENT(ASurvivalHUD*, HUD)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args)
    {
        Player = Args._Character;
        LoadGearBrushes(GearIcons, GearAtlas);
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Icons); ++Index)
        {
            const auto Item = static_cast<ESurvivalItem>(Index);
            Icons[Index].SetResourceObject(Args._HUD->GetItemIcon(Item));
            Icons[Index].SetUVRegion(Args._HUD->GetItemIconUV(Item));
            Icons[Index].DrawAs = ESlateBrushDrawType::Image;
        }
        const int32 Count = Player->GetContainerSlotCount();
        const int32 Columns = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(Count))));
        auto Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(2));
        for (int32 Index = 0; Index < Count; ++Index)
            Grid->AddSlot(Index % Columns, Index / Columns)
                [SNew(SLootSlot).Character(Player.Get()).Index(Index).Icons(Icons).GearIcons(GearIcons.GetData())];

        ChildSlot
        [
            SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(PanelColor).Padding(8)
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[Grid]
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,6,0,0)
                [SNew(STextBlock).Text(FText::FromString(TEXT("Press R to take all")))]]
        ];
    }
    bool SupportsKeyboardFocus() const override { return true; }

    FReply OnKeyDown(const FGeometry&, const FKeyEvent& Event) override
    {
        if (!Player.IsValid()) return FReply::Unhandled();
        const FKey Key = Event.GetKey();
        if (Key != EKeys::R && Key != EKeys::I && Key != EKeys::Escape && Key != EKeys::E)
            return FReply::Unhandled();
        if (!Event.IsRepeat())
        {
            if (Key == EKeys::R) Player->TakeContainerLoot();
            else if (Key == EKeys::I) Player->ToggleInventory();
            else Player->CloseContainer();
        }
        return FReply::Handled();
    }
private:
    TWeakObjectPtr<ASurvivalCharacter> Player;
    TStrongObjectPtr<UTexture2D> GearAtlas;
    TArray<FSlateBrush> GearIcons;
    FSlateBrush Icons[static_cast<int32>(ESurvivalItem::None)];
};

const FKey SlotKeys[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six};
static_assert(UE_ARRAY_COUNT(SlotKeys) == ASurvivalCharacter::HotbarSize);
}

TSharedRef<SWidget> MakeSurvivalLootGrid(ASurvivalCharacter& Character, ASurvivalHUD& HUD)
{
    return SNew(SLootGrid).Character(&Character).HUD(&HUD);
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
    Page = Player.IsValid() && Player->HasOpenContainer() ? EPage::Pack : EPage::Crafting;
    LoadGearBrushes(GearIcons, GearAtlas);
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
    }
    // The grid holds the most cells the pack can ever grow to. Rows past the pack's current size stay collapsed.
    for (int32 Index = 0; Index < ASurvivalCharacter::MaxPackCells; ++Index)
    {
        Items->AddSlot(Index % ASurvivalCharacter::PackRowWidth, Index / ASurvivalCharacter::PackRowWidth)
        [
            SNew(SBox).Visibility_Lambda([this, Index]()
            {
                return Player.IsValid() && Index < Player->GetPackSlotCount() ? EVisibility::Visible : EVisibility::Collapsed;
            })
            [SNew(SPackCell).Player(Player.Get()).Index(Index).Belt(false).Icons(Icons).GearIcons(GearIcons.GetData())]
        ];
    }
    TSharedRef<SHorizontalBox> Slots = SNew(SHorizontalBox);
    for (int32 Slot = 0; Slot < ASurvivalCharacter::HotbarSize; ++Slot)
    {
        Slots->AddSlot().FillWidth(1).Padding(2)
        [SNew(SPackCell).Player(Player.Get()).Index(Slot).Belt(true).Icons(Icons).GearIcons(GearIcons.GetData())];
    }

    TSharedRef<SHorizontalBox> Tabs = SNew(SHorizontalBox);
    const TCHAR* PageNames[] = {TEXT("Backpack"), TEXT("Crafting"), TEXT("Skills"), TEXT("Character"), TEXT("Stats")};
    const TCHAR* PageIcons[] = {TEXT("Backpack"), TEXT("Workshop"), TEXT("Skills"), TEXT("Survivor"), TEXT("Athletics")};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(PageNames); ++Index)
    {
        const auto Tab = static_cast<EPage>(Index);
        Tabs->AddSlot().AutoWidth().Padding(0, 0, 5, 0)
        [
            SNew(SButton).ButtonStyle(&MenuButtonStyle()).ContentPadding(FMargin(15, 10))
            .ButtonColorAndOpacity_Lambda([this, Tab]() { return FSlateColor(Page == Tab ? Chosen : Tile); })
            .OnClicked_Lambda([this, Tab]() { Page = Tab; SetWorkbench(false); return FReply::Handled(); })
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

    TSharedRef<SVerticalBox> Profile = SNew(SVerticalBox);
    Portrait.SetResourceObject(Player->GetInventoryPortrait());
    Portrait.DrawAs = ESlateBrushDrawType::Image;
    Portrait.SetImageSize(FVector2D(180,240));
    TSharedRef<SVerticalBox> LeftSlots = SNew(SVerticalBox);
    TSharedRef<SVerticalBox> RightSlots = SNew(SVerticalBox);
    const TCHAR* WearSlots[] = {TEXT("Helmet"),TEXT("Eyes"),TEXT("Face"),TEXT("Torso"),TEXT("Body"),
                                TEXT("Vest"),TEXT("Hands"),TEXT("Belt"),TEXT("Legs")};
    for (int32 I = 0; I < UE_ARRAY_COUNT(WearSlots); ++I)
    {
        auto Column = I < 5 ? LeftSlots : RightSlots;
        Column->AddSlot().FillHeight(1).Padding(2)[SNew(SWearSlot).Player(Player.Get()).WearSlot(FString(WearSlots[I])).GearIcons(GearIcons.GetData())];
    }
    Profile->AddSlot().AutoHeight().Padding(0,0,0,12)[SectionLabel(TEXT("STATUS"))];
    const TCHAR* VitalNames[] = {TEXT("Health"), TEXT("Stamina"), TEXT("Food"), TEXT("Water")};
    const FLinearColor VitalColors[] = {{.65f,.21f,.16f}, {.35f,.49f,.20f}, {.67f,.44f,.21f}, {.18f,.43f,.56f}};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(VitalNames); ++Index)
    {
        auto Value = [this, Index]() -> float
        {
            if (!Player.IsValid()) { return 0.f; }
            const FSurvivalVitals& Vitals = Player->GetVitals();
            switch (Index)
            {
            case 0: return Player->GetHealthPoints();
            case 1: return Vitals.Stamina;
            case 2: return Vitals.Hunger;
            default: return Vitals.Thirst;
            }
        };
        Profile->AddSlot().AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(FText::FromString(VitalNames[Index]))]
            + SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text_Lambda([Value]() {
                return FText::AsNumber(FMath::RoundToInt(Value()));
            })]
        ];
        Profile->AddSlot().AutoHeight().Padding(0, 4, 0, 16)
        [
            SNew(SBox).HeightOverride(5)[SNew(SProgressBar).FillColorAndOpacity(VitalColors[Index])
                .Percent_Lambda([Value]() -> TOptional<float> { return Value() / FSurvivalVitals::Max; })]
        ];
    }

    Profile->AddSlot().AutoHeight().Padding(0,8)[SectionLabel(TEXT("CONDITIONS"))];
    Profile->AddSlot().AutoHeight()[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]{
        if (!Player.IsValid()) return FText::GetEmpty();
        const auto& Vitals = Player->GetVitals();
        TArray<FString> Conditions;
        if (!Vitals.CanSprint()) Conditions.Add(TEXT("Exhausted"));
        if (Vitals.Hunger <= 0) Conditions.Add(TEXT("Starving"));
        if (Vitals.Thirst <= 0) Conditions.Add(TEXT("Dehydrated"));
        return FText::FromString(Conditions.IsEmpty() ? FString(TEXT("No active conditions")) : FString::Join(Conditions, TEXT("\n")));
    })];
    const auto ModifyOpen = [this]() { return bModifying && Player.IsValid(); };
    TSharedRef<SVerticalBox> ModifySlots = SNew(SVerticalBox);
    for (int32 PartSlot = 0; PartSlot < FSurvivalWeaponAssembly::PartCount; ++PartSlot)
    {
        ModifySlots->AddSlot().AutoHeight().Padding(0, 0, 0, 6)
        [SNew(SPartSlot).Player(Player.Get()).PartSlot(PartSlot).Icons(Icons)];
    }
    TSharedRef<SVerticalBox> SpareRows = SNew(SVerticalBox);
    for (int32 Slot = 0; Slot < ASurvivalCharacter::MaxPackCells; ++Slot)
    {
        SpareRows->AddSlot().AutoHeight().Padding(0, 0, 0, 4)
        [SNew(SSpareRow).Player(Player.Get()).PackSlot(Slot).Icons(Icons)];
    }
    TSharedRef<SVerticalBox> Mounts = SNew(SVerticalBox);
    for (int32 Mod = 0; Mod < UE_ARRAY_COUNT(WeaponMods); ++Mod)
    {
        Mounts->AddSlot().AutoHeight().Padding(0,2)
        [SNew(SButton).ButtonStyle(&MenuButtonStyle()).ButtonColorAndOpacity(Tile)
            .Text_Lambda([this,Mod]{ return FText::FromString(FString::Printf(TEXT("%s %s%s"),
                Player.IsValid() && Player->HasWeaponMod(Player->InspectedWeapon, Mod) ? TEXT("Remove") : TEXT("Fit"),
                WeaponMods[Mod].Name, Player.IsValid() && Player->HasWeaponMod(Player->InspectedWeapon,Mod) ? TEXT("") : Mod == 0 ? TEXT("  ·  1 cloth") : TEXT("  ·  1 stone"))); })
            .IsEnabled_Lambda([this,Mod]{ return Player.IsValid() && Player->CanToggleWeaponMod(Player->InspectedWeapon,Mod); })
            .OnClicked_Lambda([this,Mod]{ if (Player.IsValid()) Player->ToggleWeaponMod(Player->InspectedWeapon,Mod); return FReply::Handled(); })];
    }
    const TSharedRef<SWidget> ModifyPanel = SNew(SBorder)
        .Visibility_Lambda([ModifyOpen]() { return ModifyOpen() ? EVisibility::Visible : EVisibility::Collapsed; })
        .BorderImage(WhiteBrush()).BorderBackgroundColor(PanelColor).Padding(FMargin(16))
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)[SectionLabel(TEXT("WEAPON WORKBENCH"))]

            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
            [
                SNew(STextBlock).ColorAndOpacity(FSlateColor(Ink)).AutoWrapText(true).Text_Lambda([this]()
                {
                    if (!Player.IsValid()) return FText::GetEmpty();
                    const auto Weapon = Player->InspectedWeapon;
                    const auto& Assembly = Player->GetWeaponAssembly(Weapon);
                    return FText::FromString(Assembly.IsComplete()
                        ? FString::Printf(TEXT("%s   ·   Quality %d"), SurvivalItemName(Weapon), Assembly.GetQuality())
                        : FString::Printf(TEXT("%s   ·   Incomplete. Install all four parts to fire or reload."), SurvivalItemName(Weapon)));
                })
            ]
            + SVerticalBox::Slot().FillHeight(1)
            [
                SNew(SScrollBox)
                + SScrollBox::Slot()[ModifySlots]
                + SScrollBox::Slot().Padding(0, 8)[Mounts]
                + SScrollBox::Slot()[SectionLabel(TEXT("SPARE PARTS"))]
                + SScrollBox::Slot()[SpareRows]
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(STextBlock).ColorAndOpacity(FSlateColor(Muted)).AutoWrapText(true)
                .Text(FText::FromString(TEXT("No spare parts in your pack. Scavenge for more.")))
                .Visibility_Lambda([this]() { return Player.IsValid() && CountSpareParts(Player.Get(), Player->InspectedWeapon) == 0 ? EVisibility::Visible : EVisibility::Collapsed; })
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(STextBlock).ColorAndOpacity(FSlateColor(Accent)).AutoWrapText(true)
                .Text(FText::FromString(TEXT("Finish reloading to change parts.")))
                .Visibility_Lambda([this]()
                {
                    return Player.IsValid() && !Player->CanInspectWeapon(Player->InspectedWeapon)
                        ? EVisibility::Visible : EVisibility::Collapsed;
                })
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 10)
            [
                SNew(STextBlock).ColorAndOpacity(FSlateColor(Muted)).AutoWrapText(true)
                .Text(FText::FromString(TEXT("Drag a compatible part onto its slot. Removed or replaced parts return to the backpack.")))
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SButton).ButtonStyle(&MenuButtonStyle()).ButtonColorAndOpacity(Chosen).HAlign(HAlign_Center)
                .Text(FText::FromString(TEXT("Done")))
                .OnClicked_Lambda([this]() { SetWorkbench(false); return FReply::Handled(); })
            ]
        ];

    ChildSlot
    .HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(24.0f)
    [
        SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
        [
            SNew(SBox).WidthOverride(1280.0f).HeightOverride(654.0f)
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
                    [SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth()
                        [SNew(SBox).WidthOverride(160).Visibility_Lambda([this]{return Page == EPage::Gear ? EVisibility::Visible : EVisibility::Collapsed;})
                            [SNew(SVerticalBox)
                                + SVerticalBox::Slot().AutoHeight().Padding(8)[SectionLabel(TEXT("CHARACTER"))]
                                + SVerticalBox::Slot().AutoHeight().Padding(8)[SNew(STextBlock).Text_Lambda([this]{ return Player.IsValid() ? FText::FromString(FString::Printf(TEXT("Level %d\nLoot stage %d"),Player->GetProgression().CharacterLevel, Player->GetLootGameStage())) : FText::GetEmpty(); })]
                                + SVerticalBox::Slot().AutoHeight().Padding(8)[SectionLabel(TEXT("QUESTS"))]
                                + SVerticalBox::Slot().AutoHeight().Padding(8)[SNew(STextBlock).Text(FText::FromString(TEXT("No active quests")))]]]
                        + SHorizontalBox::Slot().FillWidth(1)
                        [SNew(SVerticalBox)
                            + SVerticalBox::Slot().FillHeight(2)
                            [SNew(SOverlay).Clipping(EWidgetClipping::ClipToBounds)
                                + SOverlay::Slot()[SNew(SHorizontalBox).Visibility_Lambda([this]{return Page == EPage::Crafting ? EVisibility::Visible : EVisibility::Collapsed;})
                                    + SHorizontalBox::Slot().FillWidth(.45f).Padding(4)[MakeRecipeBrowser()]
                                    + SHorizontalBox::Slot().FillWidth(.55f).Padding(4)[MakeRecipeDetails()]]
                                + SOverlay::Slot()[MakeSkillsPanel()]
                                + SOverlay::Slot()[MakeStatsPanel()]
                                + SOverlay::Slot()[SNew(SHorizontalBox).Visibility_Lambda([this]{return Page == EPage::Gear ? EVisibility::Visible : EVisibility::Collapsed;})
                                    + SHorizontalBox::Slot().FillWidth(1)[LeftSlots]
                                    + SHorizontalBox::Slot().FillWidth(1.4f)[SNew(SPortraitOrbit).Player(Player.Get()).Brush(&Portrait)]
                                    + SHorizontalBox::Slot().FillWidth(1)[RightSlots]]
                                + SOverlay::Slot()
                                [
                                    SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
                                    .Visibility_Lambda([ModifyOpen]{return ModifyOpen() ? EVisibility::Visible : EVisibility::Collapsed;})
                                    [SNew(SImage).Image(&Portrait)]
                                ]
                                + SOverlay::Slot()
                                [
                                    SNew(SScrollBox)
                                    .Visibility_Lambda([this, ModifyOpen]{
                                        return Page == EPage::Pack && !ModifyOpen() ? EVisibility::Visible : EVisibility::Collapsed;
                                    })
                                    + SScrollBox::Slot()
                                    [
                            SNew(SBorder).Visibility_Lambda([ModifyOpen]() { return ModifyOpen() ? EVisibility::Collapsed : EVisibility::Visible; })
                            .BorderImage(WhiteBrush()).BorderBackgroundColor(PanelColor).Padding(FMargin(20))
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
                                        if (!Player.IsValid()) return FText::GetEmpty();
                                        const int32 Gear = Player->GetPackCell(Player->GetSelectedPackSlot()).GearIndex;
                                        return FText::FromString(Gear == INDEX_NONE ? SurvivalItemName(Player->GetSelectedInventoryItem()) : SurvivalGear[Gear].Label);
                                    })
                                ]
                                + SVerticalBox::Slot().AutoHeight()
                                [
                                    SNew(STextBlock).ColorAndOpacity(FSlateColor(Muted)).Text_Lambda([this]()
                                    {
                                        if (!Player.IsValid()) { return FText::GetEmpty(); }
                                        const ESurvivalItem Item = Player->GetSelectedInventoryItem();
                                        const FSurvivalPackCell Cell = Player->GetPackCell(Player->GetSelectedPackSlot());
                                        if (Cell.GearIndex != INDEX_NONE)
                                            return FText::FromString(FString::Printf(TEXT("%s  ·  %d in stack"), SurvivalGear[Cell.GearIndex].Slot, Cell.Count));
                                        if (SurvivalItemHasQuality(Item))
                                        {
                                            return FText::FromString(FString::Printf(TEXT("Quality %d"), Cell.Quality));
                                        }
                                        return FText::FromString(Cell.IsEmpty() || Cell.Item != Item
                                            ? FString::Printf(TEXT("%d carried"), Player->GetItemCount(Item))
                                            : FString::Printf(TEXT("Stack %d of %d   /   %d carried"), Cell.Count,
                                                SurvivalItemMaxStack(Item), Player->GetItemCount(Item)));
                                    })
                                ]
                                + SVerticalBox::Slot().AutoHeight().Padding(0, 18, 0, 0)
                                [
                                    SNew(STextBlock).ColorAndOpacity(FSlateColor(Ink)).AutoWrapText(true)
                                    .Text_Lambda([this]()
                                    {
                                        const ASurvivalCharacter* Character = Player.Get();
                                        if (!Character) { return FText::GetEmpty(); }
                                        const auto Item = Character->GetSelectedInventoryItem();
                                        const auto Weapon = AssemblyWeapon(Item);
                                        if (IsModifiable(Item) && Character->GetItemCount(Weapon) > 0)
                                        {
                                            const auto& Assembly = Character->GetWeaponAssembly(Weapon);
                                            return FText::FromString(Assembly.IsComplete()
                                                ? FString::Printf(TEXT("%s assembly quality %d"), SurvivalItemName(Weapon), Assembly.GetQuality())
                                                : FString::Printf(TEXT("%s is missing parts"), SurvivalItemName(Weapon)));
                                        }
                                        return FText::GetEmpty();
                                    })
                                ]
                                + SVerticalBox::Slot().AutoHeight()
                                [SNew(SButton).Text(FText::FromString(TEXT("Drop one garment")))
                                    .Visibility_Lambda([this]{ return Player.IsValid() && Player->GetPackCell(Player->GetSelectedPackSlot()).GearIndex != INDEX_NONE ? EVisibility::Visible : EVisibility::Collapsed; })
                                    .OnClicked_Lambda([this]{ if (Player.IsValid()) Player->ServerDropGear(Player->GetPackCell(Player->GetSelectedPackSlot()).GearIndex); return FReply::Handled(); })]
                                + SVerticalBox::Slot().AutoHeight()
                                [SNew(SButton).Text(FText::FromString(TEXT("Drop stack")))
                                    .Visibility_Lambda([this]{ return Player.IsValid() && Player->CanDropSelection() ? EVisibility::Visible : EVisibility::Collapsed; })
                                    .OnClicked_Lambda([this]{ if (Player.IsValid()) Player->DropSelection(); return FReply::Handled(); })]
                                + SVerticalBox::Slot().AutoHeight()
                                [SNew(SButton).Text(FText::FromString(TEXT("Split stack")))
                                    .Visibility_Lambda([this]{ return Player.IsValid() && Player->GetPackCell(Player->GetSelectedPackSlot()).Count > 1 ? EVisibility::Visible : EVisibility::Collapsed; })
                                    .OnClicked_Lambda([this]{ if (Player.IsValid()) Player->SplitSelection(); return FReply::Handled(); })]
                                + SVerticalBox::Slot().FillHeight(1)[SNew(SSpacer)]
                                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
                                [
                                    SNew(SButton).ButtonStyle(&MenuButtonStyle()).ButtonColorAndOpacity(Chosen)
                                    .HAlign(HAlign_Center).Text_Lambda([this]()
                                    {
                                        if (Player.IsValid() && Player->GetPackCell(Player->GetSelectedPackSlot()).GearIndex != INDEX_NONE) return FText::FromString(TEXT("Equip"));
                                        const bool bModify = Player.IsValid() && IsModifiable(Player->GetSelectedInventoryItem());
                                        return FText::FromString(bModify ? TEXT("Modify  [M]") : TEXT("Use selected item"));
                                    })
                                    .IsEnabled_Lambda([this]()
                                    {
                                        if (!Player.IsValid()) { return false; }
                                        const ESurvivalItem Item = Player->GetSelectedInventoryItem();
                                        if (Player->GetPackCell(Player->GetSelectedPackSlot()).GearIndex != INDEX_NONE) return true;
                                        return IsModifiable(Item) ? Player->CanInspectWeapon(Item) : Player->CanUseItem(Item);
                                    })
                                    .OnClicked_Lambda([this]()
                                    {
                                        if (ASurvivalCharacter* Character = Player.Get())
                                        {
                                            const ESurvivalItem Item = Character->GetSelectedInventoryItem();
                                            const int32 Gear = Character->GetPackCell(Character->GetSelectedPackSlot()).GearIndex;
                                            if (Gear != INDEX_NONE) Character->ServerSetGearEquipped(Gear,true);
                                            else if (IsModifiable(Item)) { SetWorkbench(Character->CanInspectWeapon(Item)); }
                                            else { Character->UseItem(Item); }
                                        }
                                        return FReply::Handled();
                                    })
                                ]
                                + SVerticalBox::Slot().AutoHeight()
                                [
                                    SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FSlateColor(Muted))
                                    .Text(FText::FromString(TEXT("Drag to move or merge. Ctrl-drag splits a stack. Shift-click moves a usable stack to the toolbelt. Press M to inspect a weapon.")))
                                ]
                            ]]]
                            ]
                            + SVerticalBox::Slot().FillHeight(1).Padding(4)
                            [SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(PanelColor).Padding(6)
                                [SNew(SVerticalBox)
                                    + SVerticalBox::Slot().AutoHeight()[SectionLabel(TEXT("BACKPACK   ·   SELECT GEAR + E TO EQUIP"))]
                                    + SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox) + SScrollBox::Slot()[Items]]
                                ]]
                            + SVerticalBox::Slot().AutoHeight()[SectionLabel(TEXT("TOOLBELT"))]
                            + SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(48)[Slots]]
                        ]
                        + SHorizontalBox::Slot().AutoWidth().Padding(8,0,0,0)
                        [SNew(SBox).WidthOverride(360)
                            .Visibility_Lambda([ModifyOpen]{ return ModifyOpen() ? EVisibility::Visible : EVisibility::Collapsed; })
                            [ModifyPanel]]
                        + SHorizontalBox::Slot().AutoWidth()
                        [SNew(SBox).WidthOverride(150).Padding(8).Visibility_Lambda([this]{return Page == EPage::Gear ? EVisibility::Visible : EVisibility::Collapsed;})[Profile]]
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
    const bool bInTree = !Info.IsPractice() && FCString::Strcmp(Tree, Info.Category) == 0;
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

TSharedRef<SWidget> SSurvivalInventoryMenu::MakeStatsPanel()
{
    auto PracticeGrid = SNew(SUniformGridPanel).SlotPadding(FMargin(2));
    int32 Cell = 0;
    for (int32 Index = 0; Index < FSurvivalProgression::EntryCount; ++Index)
    {
        const ETraining Entry = static_cast<ETraining>(Index);
        const auto& Info = GetTrainingInfo(Entry);
        if (!Info.IsPractice()) continue;
        PracticeGrid->AddSlot(Cell % 3, Cell / 3)
        [
            SNew(SButton).ButtonStyle(&MenuButtonStyle()).ContentPadding(FMargin(5, 2))
            .ButtonColorAndOpacity_Lambda([this, Entry]{ return FSlateColor(Entry == SelectedStat ? Chosen : Tile); })
            .Text_Lambda([this, Entry]{
                const auto& Stat = GetTrainingInfo(Entry);
                if (!Stat.bAvailable) return FText::FromString(FString::Printf(TEXT("%s  ·  Planned"), Stat.Name));
                return FText::FromString(FString::Printf(TEXT("%s  ·  %d"), Stat.Name,
                    Player.IsValid() ? Player->GetProgression().Rank(Entry) : 0));
            })
            .OnClicked_Lambda([this, Entry]{ SelectedStat = Entry; return FReply::Handled(); })
        ];
        ++Cell;
    }

    return SNew(SHorizontalBox)
        .Visibility_Lambda([this]{ return Page == EPage::Stats ? EVisibility::Visible : EVisibility::Collapsed; })
        + SHorizontalBox::Slot().FillWidth(.70f).Padding(4)
        [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)[PracticeGrid]]
        + SHorizontalBox::Slot().FillWidth(.30f).Padding(4)
        [
            SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(PanelColor).Padding(14)
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[SectionLabel(TEXT("LEARN BY DOING"))]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 8)
                [SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]{
                    const auto& Stat = GetTrainingInfo(SelectedStat);
                    if (!Player.IsValid()) return FText::GetEmpty();
                    const auto& Progress = Player->GetProgression();
                    return FText::FromString(FString::Printf(TEXT("%s\nRank %d / %d\n\n%s\n\n%s"), Stat.Name,
                        Progress.Rank(SelectedStat), Stat.MaxRank, Stat.Description,
                        *(Stat.bAvailable ? FString::Printf(TEXT("%d / %d practice XP"), Progress.Practice(SelectedStat), Progress.PracticeRequired(SelectedStat)) : FString(TEXT("Planned")))));
                })]]
        ];
}

TSharedRef<SWidget> SSurvivalInventoryMenu::MakeSkillsPanel()
{
    TSharedRef<SVerticalBox> Navigation = SNew(SVerticalBox);
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(TrainingTrees); ++Index)
    {
        if (Index == 0)
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
    TSharedRef<SWrapBox> Nodes = SNew(SWrapBox).UseAllottedSize(true);
    TrainingRows->AddSlot()[Nodes];
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
            Nodes->AddSlot().Padding(2)
            [
                SNew(SBox).WidthOverride(125).HeightOverride(86)
                .Visibility_Lambda([this, Entry]() { return MatchesTraining(Entry) ? EVisibility::Visible : EVisibility::Collapsed; })
                [
                    SNew(SButton).ButtonStyle(&MenuButtonStyle()).HAlign(HAlign_Center).ContentPadding(3)
                    .ButtonColorAndOpacity_Lambda([this, Index]() { return FSlateColor(SelectedTraining == Index ? Chosen : Tile); })
                    .OnClicked_Lambda([this, Index]() { SelectTraining(Index); return FReply::Handled(); })
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                        [SNew(SImage).Image(FindThemeIcon(Info.Icon)).ColorAndOpacity(bAvailable ? Accent : Muted)]
                        + SVerticalBox::Slot().AutoHeight()
                        [SNew(STextBlock).Text(FText::FromString(Info.Name)).Justification(ETextJustify::Center)
                            .WrapTextAt(115.f).ColorAndOpacity(bAvailable ? Ink : Muted)]
                        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                        [SNew(STextBlock).ColorAndOpacity(Muted).Text_Lambda([this, Entry]() {
                            const FTrainingInfo& Skill = GetTrainingInfo(Entry);
                            if (!Skill.bAvailable) { return FText::FromString(TEXT("Planned")); }
                            return FText::FromString(FString::Printf(TEXT("%d / %d"),
                                Player.IsValid() ? Player->GetProgression().Rank(Entry) : 0, Skill.MaxRank));
                        })]
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
                SNew(SBox).WidthOverride(148)
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
    TArray<FString> RecipeCategories = {TEXT("All")};
    for (const FSurvivalRecipe& Recipe : GetSurvivalRecipes())
    {
        RecipeCategories.AddUnique(FString(Recipe.Category));
    }
    for (const FString& Label : RecipeCategories)
    {
        Categories->AddSlot().FillWidth(1).Padding(0, 0, 4, 0)
        [
            SNew(SButton).ButtonStyle(&MenuButtonStyle()).HAlign(HAlign_Center).Text(FText::FromString(Label))
            .ButtonColorAndOpacity_Lambda([this, Label]()
            {
                return FSlateColor(Category == Label || (Category.IsEmpty() && Label == TEXT("All"))
                    ? Chosen : Tile);
            })
            .OnClicked_Lambda([this, Label]()
            {
                Category = Label == TEXT("All") ? TEXT("") : Label;
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
            SAssignNew(RecipeFilterBox, SSearchBox).HintText(FText::FromString(TEXT("Find recipe")))
            .OnTextChanged_Lambda([this](const FText& Text)
            {
                SearchText = Text.ToString().TrimStartAndEnd();
                SelectFirstMatchingRecipe();
            })
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)[Categories]
        + SVerticalBox::Slot().FillHeight(1)[Rows];
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
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(0,0,0,8)
            [SNew(SImage).DesiredSizeOverride(FVector2D(65,65)).Image_Lambda([this]() {
                const auto Recipes = GetSurvivalRecipes();
                return Recipes.IsValidIndex(SelectedRecipe) ? FindIcon(Recipes[SelectedRecipe].Output) : nullptr;
            })]
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
        const bool bSearching = (RecipeFilterBox && RecipeFilterBox->HasFocusedDescendants())
            || (TrainingFilterBox && TrainingFilterBox->HasFocusedDescendants());
        if (bSearching && Event.GetKey() != EKeys::Escape) return FReply::Unhandled();
        if (Event.GetKey() == EKeys::R && Character->HasOpenContainer())
        {
            if (!Event.IsRepeat()) Character->TakeContainerLoot();
            return FReply::Handled();
        }
        const int32 Gear = Character->GetPackCell(Character->GetSelectedPackSlot()).GearIndex;
        if (Gear != INDEX_NONE && Event.GetKey() == EKeys::E)
        {
            if (!Event.IsRepeat()) Character->ServerSetGearEquipped(Gear, true);
            return FReply::Handled();
        }
        if (Event.GetKey() == EKeys::M && IsModifiable(Character->GetSelectedInventoryItem()) && Character->CanInspectWeapon(Character->GetSelectedInventoryItem()))
        {
            if (!Event.IsRepeat()) { Page = EPage::Pack; SetWorkbench(!bModifying); }
            return FReply::Handled();
        }
        if (bModifying && Event.GetKey() == EKeys::Escape)
        {
            if (!Event.IsRepeat())
            {
                SetWorkbench(false);
            }
            return FReply::Handled();
        }
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


void SSurvivalInventoryMenu::SetWorkbench(bool bOpen)
{
    bModifying = bOpen;
    if (Player.IsValid())
    {
        Player->InspectWeapon(bOpen ? AssemblyWeapon(Player->GetSelectedInventoryItem()) : ESurvivalItem::None);
        Player->ShowWeaponPortrait(bOpen, Page == EPage::Gear);
        Portrait.SetResourceObject(Player->GetInventoryPortrait());
    }
}
