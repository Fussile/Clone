#include "SurvivalHUD.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "SurvivalBuildChunk.h"
#include "SurvivalCharacter.h"
#include "SurvivalGameMode.h"
#include "SurvivalInventoryMenu.h"
#include "SurvivalSupplyCrate.h"

namespace
{
const FLinearColor Panel(0.02f, 0.04f, 0.05f, 0.88f);
const FLinearColor Cyan(0.42f, 0.88f, 1.0f);
const FLinearColor Muted(0.72f, 0.76f, 0.78f);
const FLinearColor Blocked(1.0f, 0.51f, 0.36f);
}

void ASurvivalHUD::BeginPlay()
{
    Super::BeginPlay();
    static const TCHAR* Names[] = {
        TEXT("Sidearm"), TEXT("Food"), TEXT("Water"), TEXT("Bandage"), TEXT("Timber"),
        TEXT("Stone"), TEXT("Cloth"), TEXT("Rounds")
    };
    static_assert(UE_ARRAY_COUNT(Names) == static_cast<int32>(ESurvivalItem::None));
    for (const TCHAR* Name : Names)
    {
        const FString Path = FString::Printf(TEXT("/Game/Art/Crossroads/Inventory/T_Item_%s.T_Item_%s"), Name, Name);
        ItemIcons.Add(LoadObject<UTexture2D>(nullptr, *Path, nullptr, LOAD_NoWarn));
    }
}

UTexture2D* ASurvivalHUD::GetItemIcon(ESurvivalItem Item) const
{
    const int32 Index = static_cast<int32>(Item);
    return ItemIcons.IsValidIndex(Index) ? ItemIcons[Index].Get() : nullptr;
}

void ASurvivalHUD::SetInventoryVisible(ASurvivalCharacter& Character, bool bVisible)
{
    ULocalPlayer* LocalPlayer = PlayerOwner ? PlayerOwner->GetLocalPlayer() : nullptr;
    UGameViewportClient* Viewport = LocalPlayer ? LocalPlayer->ViewportClient : nullptr;
    if (!LocalPlayer || !Viewport || bVisible == InventoryWidget.IsValid())
    {
        return;
    }

    if (bVisible)
    {
        InventoryWidget = SNew(SSurvivalInventoryMenu).Player(&Character).HUD(this);
        Viewport->AddViewportWidgetForPlayer(LocalPlayer, InventoryWidget.ToSharedRef(), 10);
        FInputModeGameAndUI Mode;
        Mode.SetWidgetToFocus(InventoryWidget);
        Mode.SetHideCursorDuringCapture(false);
        PlayerOwner->SetInputMode(Mode);
    }
    else
    {
        RemoveInventoryWidget();
        PlayerOwner->SetInputMode(FInputModeGameOnly());
    }
    PlayerOwner->bShowMouseCursor = bVisible;
    PlayerOwner->SetIgnoreLookInput(bVisible);
    PlayerOwner->SetIgnoreMoveInput(bVisible);
}

void ASurvivalHUD::RemoveInventoryWidget()
{
    if (InventoryWidget && PlayerOwner)
    {
        if (ULocalPlayer* LocalPlayer = PlayerOwner->GetLocalPlayer(); LocalPlayer && LocalPlayer->ViewportClient)
        {
            LocalPlayer->ViewportClient->RemoveViewportWidgetForPlayer(LocalPlayer, InventoryWidget.ToSharedRef());
        }
    }
    InventoryWidget.Reset();
}

void ASurvivalHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    RemoveInventoryWidget();
    Super::EndPlay(EndPlayReason);
}

void ASurvivalHUD::DrawHotbar(const ASurvivalCharacter& Character)
{
    constexpr float SlotSize = 55.0f;
    constexpr float Gap = 4.0f;
    const float Start = (Canvas->SizeX - (SlotSize + Gap) * ASurvivalCharacter::HotbarSize + Gap) * 0.5f;
    const float Y = Canvas->SizeY - SlotSize - 18.0f;
    for (int32 Index = 0; Index < ASurvivalCharacter::HotbarSize; ++Index)
    {
        const float X = Start + Index * (SlotSize + Gap);
        const bool bSelected = Index == Character.GetActiveHotbarSlot();
        if (bSelected)
        {
            DrawRect(Cyan, X - 2.0f, Y - 2.0f, SlotSize + 4.0f, SlotSize + 4.0f);
        }
        DrawRect(Panel, X, Y, SlotSize, SlotSize);
        const ESurvivalItem Item = Character.GetHotbarItem(Index);
        const int32 Count = Character.GetItemCount(Item);
        if (UTexture2D* Icon = GetItemIcon(Item))
        {
            DrawTexture(Icon, X + 7.0f, Y + 7.0f, 40.0f, 40.0f, 0, 0, 1, 1,
                FLinearColor(1, 1, 1, Count > 0 ? 1.0f : 0.25f));
        }
        DrawText(FString::FromInt(Index + 1), bSelected ? Cyan : Muted, X + 5.0f, Y + 3.0f);
        if (Item != ESurvivalItem::None)
        {
            DrawText(FString::FromInt(Count), FLinearColor::White, X + 35.0f, Y + 36.0f);
        }
    }
    const FString Active = SurvivalItemName(Character.GetHotbarItem(Character.GetActiveHotbarSlot()));
    float LabelWidth = 0.0f;
    float LabelHeight = 0.0f;
    GetTextSize(Active, LabelWidth, LabelHeight);
    DrawText(Active, FLinearColor::White, (Canvas->SizeX - LabelWidth) * 0.5f, Y - LabelHeight - 7.0f);
}

void ASurvivalHUD::DrawHUD()
{
    Super::DrawHUD();
    const ASurvivalCharacter* Character = PlayerOwner ? Cast<ASurvivalCharacter>(PlayerOwner->GetPawn()) : nullptr;
    if (!Canvas || !Character)
    {
        return;
    }

    const float CenterX = Canvas->SizeX * 0.5f;
    const float CenterY = Canvas->SizeY * 0.5f;
    if (!Character->IsAimingSidearm() && !Character->IsInventoryOpen())
    {
        DrawRect(FLinearColor::White, CenterX - 1.5f, CenterY - 1.5f, 3.0f, 3.0f);
    }
    if (Character->ShowHitMarker())
    {
        for (const float X : {-1.0f, 1.0f})
        {
            for (const float Y : {-1.0f, 1.0f})
            {
                DrawLine(CenterX + X * 4.0f, CenterY + Y * 4.0f,
                    CenterX + X * 9.0f, CenterY + Y * 9.0f, Cyan, 1.5f);
            }
        }
    }

    const FString Loadout = Character->HasSidearm()
        ? FString::Printf(TEXT("Sidearm   %d / %d%s"), Character->GetAmmo(), Character->GetReserveAmmo(),
            Character->IsReloading() ? TEXT("  Reloading") : TEXT(""))
        : TEXT("Sidearm   search the shop");
    const int32 Wave = Character->GetDefenseWave();
    DrawRect(Panel, 20.0f, 20.0f, 420.0f, Wave > 0 ? 74.0f : 54.0f);
    DrawText(FString::Printf(TEXT("Health   %d   |   %s"), Character->GetHealthPoints(), *Loadout),
        FLinearColor::White, 32.0f, 28.0f);
    if (const ASurvivalGameMode* Mode = GetWorld()->GetAuthGameMode<ASurvivalGameMode>())
    {
        const int32 Minutes = Mode->GetWorldMinutes();
        DrawText(FString::Printf(TEXT("Day %d   %02d:%02d"), Minutes / 1440 + 1, (Minutes / 60) % 24, Minutes % 60),
            FLinearColor::White, 325.0f, 28.0f);
        const int32 Temperature = FMath::RoundToInt32(Mode->GetAmbientTemperature());
        DrawText(FString::Printf(TEXT("%d C"), Temperature), Temperature < 12 ? Cyan : Muted, 325.0f, 48.0f);
    }
    const FSurvivalVitals& Vitals = Character->GetVitals();
    DrawText(FString::Printf(TEXT("Food %d%%   Water %d%%   Stamina %d%%"), FMath::CeilToInt(Vitals.Hunger), FMath::CeilToInt(Vitals.Thirst),
        FMath::CeilToInt(Vitals.Stamina)),
        Vitals.Hunger <= 10.0f || Vitals.Thirst <= 10.0f ? Blocked : Muted, 32.0f, 48.0f);

    const int32 Alive = Character->GetLivingAttackers();
    FString DefenseStatus;
    if (Alive > 0)
    {
        DefenseStatus = FString::Printf(TEXT("Wave %d / %d   |   %d remaining"),
            Wave, ASurvivalCharacter::DefenseWaveCount, Alive);
    }
    else if (Wave >= ASurvivalCharacter::DefenseWaveCount)
    {
        DefenseStatus = TEXT("Station defended   [F5] Restart");
    }
    else if (Wave > 0)
    {
        DefenseStatus = TEXT("Wave clear   [G] Next wave (+6 rounds)");
    }
    if (Wave > 0)
    {
        DrawText(DefenseStatus, Muted, 32.0f, 68.0f);
    }
    if (Character->GetHealthPoints() <= 0)
    {
        DrawText(TEXT("Downed   [F5] Restart"), Blocked, CenterX + 14.0f, CenterY + 12.0f);
        return;
    }

    if (Character->IsInventoryOpen())
    {
        return;
    }
    DrawHotbar(*Character);
    const FSurvivalMaterials Inventory = Character->GetInventory();
    const FSurvivalMaterials PlannedCost = Character->GetPlannedCost();
    const bool bHasPlans = !PlannedCost.IsEmpty();
    const float PanelHeight = bHasPlans ? 92.0f : 52.0f;
    const float PanelY = Canvas->SizeY - PanelHeight - 100.0f;
    DrawRect(Panel, 20.0f, PanelY, 322.0f, PanelHeight);
    DrawText(FString::Printf(TEXT("Inventory   %d timber / %d stone"), Inventory.Timber, Inventory.Stone),
        FLinearColor::White, 32.0f, PanelY + 10.0f);
    if (bHasPlans)
    {
        const FSurvivalMaterials Shortage = PlannedCost.MissingFrom(Inventory);
        DrawText(FString::Printf(TEXT("Plans require   %s"), *PlannedCost.ToText()), Cyan, 32.0f, PanelY + 30.0f);
        const FString Status = Shortage.IsEmpty() ? TEXT("Materials ready")
            : FString::Printf(TEXT("Still needed   %s"), *Shortage.ToText());
        DrawText(Status, Shortage.IsEmpty() ? Muted : Blocked, 32.0f, PanelY + 50.0f);
    }
    DrawText(TEXT("[B] Build   [I] Inventory   [V] View"), Muted, 32.0f, PanelY + PanelHeight - 22.0f);

    if (Character->IsBuilding())
    {
        DrawBuildMenu(*Character);
        if (const TOptional<FSurvivalBuildPlacement> Placement = Character->FindPlacement(); Placement.IsSet())
        {
            DrawPlacement(*Placement, Placement->bClear ? Cyan : Blocked);
        }
    }

    const FSurvivalPlanTarget Target = Character->FindPlan();
    if (const FSurvivalBuildPlacement* Plan = Target.GetPlan())
    {
        const FSurvivalMaterials Missing = Plan->GetPiece().Cost.MissingFrom(Inventory);
        FString Prompt = TEXT("[E] Finish   [X] Remove plan");
        FLinearColor Color = Cyan;
        if (!Missing.IsEmpty())
        {
            Prompt = FString::Printf(TEXT("Need %s   [X] Remove"), *Missing.ToText());
            Color = Blocked;
        }
        else if (!Character->CanFinish(*Plan))
        {
            Prompt = TEXT("Needs support or clearance   [X] Remove");
            Color = Blocked;
        }

        DrawText(FString::Printf(TEXT("%s   %s"), Plan->GetPiece().Name, *Plan->GetPiece().Cost.ToText()),
            FLinearColor::White, CenterX + 14.0f, CenterY + 12.0f);
        DrawText(Prompt, Color, CenterX + 14.0f, CenterY + 32.0f);
    }
    else if (const ASurvivalSupplyCrate* Crate = Character->FindSupplyCrate())
    {
        const TCHAR* Prompt = Crate->HasSidearm() ? TEXT("[E] Take sidearm and ammo") :
            (Crate->HasSupplies() ? TEXT("[E] Scavenge supplies") : TEXT("Crate empty"));
        DrawText(Prompt,
            FLinearColor::White, CenterX + 14.0f, CenterY + 12.0f);
    }
    else if (const FSurvivalFinishedPiece* Piece = Character->FindFinishedPiece())
    {
        DrawText(FString::Printf(TEXT("%s   %d / %d HP"), Piece->Placement.GetPiece().Name,
            Piece->Health, Piece->MaxHealth), FLinearColor::White, CenterX + 14.0f, CenterY + 12.0f);
        if (Character->IsBuilding())
        {
            DrawText(TEXT("[K] Dismantle"), Muted, CenterX + 14.0f, CenterY + 52.0f);
        }
        if (!Character->IsBuilding() && Piece->Door.IsValid())
        {
            DrawText(Piece->bDoorOpen ? TEXT("[E] Close door") : TEXT("[E] Open door"),
                Cyan, CenterX + 14.0f, CenterY + 32.0f);
        }
        else if (Piece->Health < Piece->MaxHealth)
        {
            const FSurvivalMaterials Cost = Piece->RepairCost();
            const bool bCanRepair = Cost.MissingFrom(Inventory).IsEmpty();
            const FString Prompt = bCanRepair ? FString::Printf(TEXT("[E] Repair for %s"), *Cost.ToText())
                : FString::Printf(TEXT("Repair needs %s"), *Cost.ToText());
            DrawText(Prompt, bCanRepair ? Cyan : Blocked, CenterX + 14.0f, CenterY + 32.0f);
        }
    }
}

void ASurvivalHUD::DrawBuildMenu(const ASurvivalCharacter& Character)
{
    const TConstArrayView<FSurvivalBuildPiece> Pieces = GetSurvivalBuildPieces();
    const float X = 20.0f;
    const float Y = 106.0f;
    constexpr float Width = 298.0f;
    constexpr float Row = 24.0f;
    const float Footer = Y + 42.0f + Row * Pieces.Num();
    DrawRect(Panel, X, Y, Width, Footer - Y + 49.0f);
    DrawText(TEXT("BUILD PLAN"), Cyan, X + 12.0f, Y + 12.0f, nullptr, 1.25f);
    for (int32 Index = 0; Index < Pieces.Num(); ++Index)
    {
        const FSurvivalBuildPiece& Piece = Pieces[Index];
        const float Top = Y + 40.0f + Row * Index;
        const bool bSelected = Index == Character.GetSelectedPiece();
        if (bSelected)
        {
            DrawRect(FLinearColor(0.10f, 0.23f, 0.29f, 0.95f), X + 7.0f, Top, Width - 14.0f, Row);
            DrawRect(Cyan, X + 7.0f, Top, 2.0f, Row);
        }
        DrawText(Piece.Name, bSelected ? FLinearColor::White : Muted, X + 15.0f, Top + 4.0f);
        DrawText(Piece.Cost.ToText(), bSelected ? Cyan : Muted, X + 193.0f, Top + 4.0f);
    }

    DrawText(TEXT("[Scroll] Select   [R] Rotate   [B] Close"), Muted, X + 12.0f, Footer + 4.0f);
    DrawText(TEXT("[LMB] Place on ground or support"), FLinearColor::White, X + 12.0f, Footer + 24.0f);
}

void ASurvivalHUD::DrawPlacement(const FSurvivalBuildPlacement& Placement, const FLinearColor& Color)
{
    for (const FBox& Box : Placement.GetPiece().Boxes)
    {
        FVector2D Corners[8];
        bool Visible[8];
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Corners); ++Index)
        {
            const FVector Offset(Index & 1 ? Box.Max.X : Box.Min.X, Index & 2 ? Box.Max.Y : Box.Min.Y,
                Index & 4 ? Box.Max.Z : Box.Min.Z);
            const FVector Point = Placement.Base + Placement.Rotation.RotateVector(Offset);
            Visible[Index] = PlayerOwner->ProjectWorldLocationToScreen(Point, Corners[Index], true);
        }

        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Corners); ++Index)
        {
            for (const int32 AxisBit : {1, 2, 4})
            {
                if (!(Index & AxisBit) && Visible[Index] && Visible[Index | AxisBit])
                {
                    const FVector2D& Start = Corners[Index];
                    const FVector2D& End = Corners[Index | AxisBit];
                    DrawLine(Start.X, Start.Y, End.X, End.Y, Color, 1.5f);
                }
            }
        }
    }
}
