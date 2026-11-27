#include "SurvivalHUD.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "SurvivalBuildChunk.h"
#include "SurvivalArsenal.h"
#include "SurvivalCharacter.h"
#include "SurvivalGameMode.h"
#include "SurvivalInventoryMenu.h"
#include "SurvivalSupplyCrate.h"
#include "SurvivalWorldDoor.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"

namespace
{
const FLinearColor Panel(0.019f, 0.025f, 0.029f, 0.9f);
const FLinearColor Alloy(0.88f, 0.47f, 0.25f);
const FLinearColor Track(0.10f, 0.12f, 0.13f, 1.0f);
const FLinearColor Cyan(0.42f, 0.88f, 1.0f);
const FLinearColor Muted(0.72f, 0.76f, 0.78f);
const FLinearColor Blocked(1.0f, 0.51f, 0.36f);
}

void ASurvivalHUD::BeginPlay()
{
    Super::BeginPlay();
    WorkshopAtlas = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Art/Crossroads/Interface/T_UI_WorkshopItems.T_UI_WorkshopItems"), nullptr, LOAD_NoWarn);
    FieldAtlas = LoadObject<UTexture2D>(nullptr,TEXT("/Game/Art/Crossroads/Interface/T_UI_FieldWeapons.T_UI_FieldWeapons"));
    FirearmAtlas = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Art/Crossroads/Interface/T_UI_Firearms.T_UI_Firearms"));
    ItemAtlas = LoadObject<UTexture2D>(nullptr,
        TEXT("/Game/Art/Crossroads/Interface/T_UI_ItemAtlas.T_UI_ItemAtlas"), nullptr, LOAD_NoWarn);
    static const TCHAR* Names[] = {
        TEXT("Sidearm"), TEXT("Food"), TEXT("Water"), TEXT("Bandage"), TEXT("Timber"),
        TEXT("Stone"), TEXT("Cloth"), TEXT("Rounds"), TEXT("TimberDoor"),
        TEXT("Rifle"), TEXT("RifleRounds"), TEXT("BuilderGuide"), TEXT("ScavengerNotes"),
        TEXT("DressingSchematic"), TEXT("FieldDressing"),
        TEXT("RifleStock"), TEXT("RifleReceiver"), TEXT("RifleBarrel"), TEXT("RifleMagazine")
    };
    static_assert(UE_ARRAY_COUNT(Names) == static_cast<int32>(ESurvivalItem::SidearmGrip));
    for (const TCHAR* Name : Names)
    {
        const FString Path = FString::Printf(TEXT("/Game/Art/Crossroads/Inventory/T_Item_%s.T_Item_%s"), Name, Name);
        ItemIcons.Add(LoadObject<UTexture2D>(nullptr, *Path, nullptr, LOAD_NoWarn));
    }
}

UTexture2D* ASurvivalHUD::GetItemIcon(ESurvivalItem Item, int32 Model) const
{
    if (IsWorkshopItem(Item)) return WorkshopAtlas.Get();
    if (IsFieldWeapon(Item)) return FieldAtlas.Get();
    if (FirearmAtlas && Model >= 0 && Model < UE_ARRAY_COUNT(Arsenal)) return FirearmAtlas.Get();
    if (ItemAtlas && Item < ESurvivalItem::RifleMagazine && Item != ESurvivalItem::TimberDoor
        && Item != ESurvivalItem::RifleStock && Item != ESurvivalItem::RifleReceiver && Item != ESurvivalItem::RifleBarrel)
    {
        return ItemAtlas.Get();
    }
    const int32 Index = static_cast<int32>(Item);
    return ItemIcons.IsValidIndex(Index) ? ItemIcons[Index].Get() : nullptr;
}

FBox2f ASurvivalHUD::GetItemIconUV(ESurvivalItem Item, int32 Model) const
{
    if (IsWorkshopItem(Item))
    {
        const int32 Cell = WorkshopIndex(Item);
        const FVector2f Start((Cell % 4) * .25f, (Cell / 4) * .25f);
        return FBox2f(Start, Start + FVector2f(.25f, .25f));
    }
    if (IsFieldWeapon(Item))
    {
        const int32 Cell = static_cast<int32>(Item) - static_cast<int32>(ESurvivalItem::PrimitiveBow);
        return FBox2f(FVector2f(Cell * .2f, 0), FVector2f((Cell + 1) * .2f, 1));
    }
    if (FirearmAtlas && Model >= 0 && Model < UE_ARRAY_COUNT(Arsenal))
    {
        const FVector2f Start((Model % 4) * .25f, (Model / 4) * .25f);
        return FBox2f(Start, Start + FVector2f(.25f, .25f));
    }
    if (ItemAtlas && GetItemIcon(Item) == ItemAtlas.Get())
    {
        const int32 Cell = static_cast<int32>(Item);
        const FVector2f Start((Cell % 5) * 100.f / 512.f, (Cell / 5) * 96.f / 512.f);
        return FBox2f(Start, Start + FVector2f(100.f / 512.f, 96.f / 512.f));
    }
    return FBox2f(FVector2f(0.0f, 0.0f), FVector2f(1.0f, 1.0f));
}

void ASurvivalHUD::SetInventoryVisible(ASurvivalCharacter& Character, bool bVisible)
{
    if (bVisible == InventoryWidget.IsValid()) return;
    RemoveMenuRoot();
    if (bVisible) InventoryWidget = SNew(SSurvivalInventoryMenu).Player(&Character).HUD(this);
    else InventoryWidget.Reset();
    RefreshMenuRoot(bVisible ? InventoryWidget : LootWidget);
}

void ASurvivalHUD::SetLootVisible(ASurvivalCharacter& Character, bool bVisible)
{
    if (bVisible == LootWidget.IsValid()) return;
    RemoveMenuRoot();
    if (bVisible) LootWidget = MakeSurvivalLootGrid(Character, *this);
    else LootWidget.Reset();
    RefreshMenuRoot(bVisible ? LootWidget : InventoryWidget);
}

void ASurvivalHUD::RefreshMenuRoot(const TSharedPtr<SWidget>& Focus)
{
    ULocalPlayer* LocalPlayer = PlayerOwner ? PlayerOwner->GetLocalPlayer() : nullptr;
    UGameViewportClient* Viewport = LocalPlayer ? LocalPlayer->ViewportClient : nullptr;
    if (!Viewport) return;

    const bool bVisible = Focus.IsValid();
    if (bVisible)
    {
        auto Panels = SNew(SHorizontalBox);
        if (LootWidget)
            Panels->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(12)[LootWidget.ToSharedRef()];
        if (InventoryWidget)
            Panels->AddSlot().AutoWidth().VAlign(VAlign_Center)[InventoryWidget.ToSharedRef()];
        MenuRoot = SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)[Panels];
        Viewport->AddViewportWidgetForPlayer(LocalPlayer, MenuRoot.ToSharedRef(), 10);

        FInputModeGameAndUI Mode;
        Mode.SetWidgetToFocus(Focus);
        Mode.SetHideCursorDuringCapture(false);
        PlayerOwner->SetInputMode(Mode);
    }
    else
    {
        PlayerOwner->SetInputMode(FInputModeGameOnly());
    }

    if (bMenuInputActive != bVisible)
    {
        bMenuInputActive = bVisible;
        PlayerOwner->bShowMouseCursor = bVisible;
        PlayerOwner->SetIgnoreLookInput(bVisible);
    }

    const bool bBlockMovement = InventoryWidget.IsValid() && !LootWidget.IsValid();
    if (bMenuBlocksMovement != bBlockMovement)
    {
        bMenuBlocksMovement = bBlockMovement;
        PlayerOwner->SetIgnoreMoveInput(bBlockMovement);
    }
}

void ASurvivalHUD::RemoveMenuRoot()
{
    if (MenuRoot && PlayerOwner)
    {
        if (ULocalPlayer* LocalPlayer = PlayerOwner->GetLocalPlayer(); LocalPlayer && LocalPlayer->ViewportClient)
            LocalPlayer->ViewportClient->RemoveViewportWidgetForPlayer(LocalPlayer, MenuRoot.ToSharedRef());
    }
    MenuRoot.Reset();
}

void ASurvivalHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    RemoveMenuRoot();
    if (PlayerOwner)
    {
        if (bMenuInputActive)
        {
            PlayerOwner->bShowMouseCursor = false;
            PlayerOwner->SetIgnoreLookInput(false);
            PlayerOwner->SetInputMode(FInputModeGameOnly());
        }
        if (bMenuBlocksMovement) PlayerOwner->SetIgnoreMoveInput(false);
    }
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
            DrawRect(Alloy, X - 1.0f, Y - 1.0f, SlotSize + 2.0f, SlotSize + 2.0f);
        }
        DrawRect(Panel, X, Y, SlotSize, SlotSize);
        const ESurvivalItem Item = Character.GetHotbarItem(Index);
        const int32 Count = Character.GetToolbeltCount(Index);
        const int32 Model = Character.GetFirearmModel(Character.GetToolbeltWeaponId(Index));
        if (UTexture2D* Icon = GetItemIcon(Item, Model))
        {
            const FBox2f UV = GetItemIconUV(Item, Model);
            const FVector2f Size = UV.GetSize();
            DrawTexture(Icon, X + 7.0f, Y + 7.0f, 40.0f, 40.0f, UV.Min.X, UV.Min.Y, Size.X, Size.Y,
                FLinearColor(1, 1, 1, Count > 0 ? 1.0f : 0.25f));
        }
        DrawText(FString::FromInt(Index + 1), bSelected ? Alloy : Muted, X + 5.0f, Y + 3.0f);
        if (Item != ESurvivalItem::None)
        {
            DrawText(FString::FromInt(Count), FLinearColor::White, X + 35.0f, Y + 36.0f);
        }
    }
    const int32 ActiveSlot = Character.GetActiveHotbarSlot();
    const FString Active = Character.GetInstanceLabel(Character.GetHotbarItem(ActiveSlot), Character.GetToolbeltWeaponId(ActiveSlot));
    float LabelWidth = 0.0f;
    float LabelHeight = 0.0f;
    GetTextSize(Active, LabelWidth, LabelHeight);
    DrawText(Active, FLinearColor::White, (Canvas->SizeX - LabelWidth) * 0.5f, Y - LabelHeight - 7.0f);
}

void ASurvivalHUD::DrawRifleScope(const ASurvivalCharacter& Character, float CenterX, float CenterY)
{
    UTextureRenderTarget2D* Lens = Character.GetScopeTexture();
    const float Blend = Character.GetScopeBlend();
    if (!Lens || Blend <= 0.0f)
    {
        return;
    }

    // Cross through black so the off-axis animated eyepiece never competes with the lens.
    DrawRect(FLinearColor(0.008f, 0.009f, 0.01f, FMath::SmoothStep(0.0f, 0.5f, Blend)),
        0.0f, 0.0f, Canvas->SizeX, Canvas->SizeY);
    const float Opacity = FMath::SmoothStep(0.5f, 1.0f, Blend);
    if (Opacity <= 0.0f)
    {
        return;
    }
    const float Radius = FMath::Min(Canvas->SizeX, Canvas->SizeY) * 0.45f * FMath::Lerp(0.78f, 1.0f, Opacity);
    const float Diameter = Radius * 2.0f;
    // Draw only the circular portion of the magnified capture.
    constexpr int32 Strips = 96;
    for (int32 Index = 0; Index < Strips; ++Index)
    {
        const float V = static_cast<float>(Index) / Strips;
        const float Center = (V + 0.5f / Strips) * 2.0f - 1.0f;
        const float HalfWidth = Radius * FMath::Sqrt(FMath::Max(0.0f, 1.0f - Center * Center));
        DrawTexture(Lens, CenterX - HalfWidth, CenterY - Radius + V * Diameter, HalfWidth * 2.0f,
            Diameter / Strips + 0.25f, 0.5f - HalfWidth / Diameter, V, HalfWidth * 2.0f / Diameter,
            1.0f / Strips, FLinearColor(Opacity, Opacity, Opacity, 1.0f), BLEND_Opaque);
    }
    const auto Ring = [this, CenterX, CenterY](float R, const FLinearColor& Color, float Thickness)
    {
        FVector2D Previous(CenterX + R, CenterY);
        for (int32 Step = 1; Step <= 96; ++Step)
        {
            const float Angle = 2.0f * PI * Step / 96.0f;
            const FVector2D Point(CenterX + R * FMath::Cos(Angle), CenterY + R * FMath::Sin(Angle));
            DrawLine(Previous.X, Previous.Y, Point.X, Point.Y, Color, Thickness);
            Previous = Point;
        }
    };
    Ring(Radius + 7.0f, FLinearColor(0.012f, 0.015f, 0.018f, Opacity), 18.0f);
    Ring(Radius + 15.0f, FLinearColor(0.19f, 0.20f, 0.20f, Opacity * .75f), 1.25f);
    const FLinearColor Reticle(0.018f, 0.022f, 0.021f, Opacity);
    const float Span = Radius * .80f;
    DrawLine(CenterX - Span, CenterY, CenterX + Span, CenterY, Reticle, 1.3f);
    DrawLine(CenterX, CenterY - Span, CenterX, CenterY + Span, Reticle, 1.3f);
    for (int32 Mark = 1; Mark <= 4; ++Mark)
    {
        const float Y = CenterY + Radius * .12f * Mark;
        const float MarkWidth = Radius * (Mark % 2 == 0 ? .052f : .029f);
        DrawLine(CenterX - MarkWidth, Y, CenterX + MarkWidth, Y, Reticle, 1.3f);
    }
    DrawRect(FLinearColor(0.92f, 0.38f, 0.24f, Opacity), CenterX - 1.2f, CenterY - 1.2f, 2.4f, 2.4f);
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
    const bool bScoped = Character->GetScopeBlend() > 0.0f && Character->GetScopeTexture();
    if (bScoped && !Character->IsInventoryOpen() && !Character->HasOpenContainer())
    {
        DrawRifleScope(*Character, CenterX, CenterY);
    }
    if (!bScoped && !Character->IsAimingSidearm() && !Character->IsInventoryOpen() && !Character->HasOpenContainer())
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

    if (Character->IsHeadUnderwater())
        DrawRect(FLinearColor(.025f, .20f, .22f, .24f), 0, 0, Canvas->SizeX, Canvas->SizeY);

    const FSurvivalVitals& Vitals = Character->GetVitals();
    const auto Meter = [this](float X, const TCHAR* Label, float Value, const FLinearColor& Color)
    {
        DrawRect(Panel, X, 20.0f, 89.0f, 40.0f);
        DrawText(FString::Printf(TEXT("%s  %d"), Label, FMath::CeilToInt(Value)), FLinearColor::White, X + 8.0f, 26.0f);
        DrawRect(Track, X + 8.0f, 49.0f, 73.0f, 3.0f);
        DrawRect(Color, X + 8.0f, 49.0f, 73.0f * FMath::Clamp(Value * .01f, 0.0f, 1.0f), 3.0f);
    };
    Meter(20.0f, TEXT("Health"), Character->GetHealthPoints(), Blocked);
    Meter(113.0f, TEXT("Stamina"), Vitals.Stamina, FLinearColor(.56f, .74f, .42f));
    Meter(206.0f, TEXT("Food"), Vitals.Hunger, Alloy);
    Meter(299.0f, TEXT("Water"), Vitals.Thirst, Cyan);

    if (Character->IsHeadUnderwater() || Character->GetOxygenPercent() < 100)
        Meter(392.0f, TEXT("O2"), Character->GetOxygenPercent(),
              Character->GetOxygenPercent() <= 25 ? Blocked : Cyan);
    if (Character->GetWetnessPercent() > 0)
        Meter(Character->IsHeadUnderwater() || Character->GetOxygenPercent() < 100 ? 485.0f : 392.0f,
              TEXT("Wet"), Character->GetWetnessPercent(), Cyan);

    const float Right = Canvas->SizeX - 20.0f;
    if (const ASurvivalGameMode* Mode = GetWorld()->GetAuthGameMode<ASurvivalGameMode>())
    {
        const int32 Minutes = Mode->GetWorldMinutes();
        const int32 Temperature = FMath::RoundToInt32(Mode->GetAmbientTemperature(Character->GetActorLocation()));
        DrawRect(Panel, Right - 145.0f, 20.0f, 145.0f, 40.0f);
        DrawText(FString::Printf(TEXT("Day %d    %02d:%02d    %d C"), Minutes / 1440 + 1,
            (Minutes / 60) % 24, Minutes % 60, Temperature), FLinearColor::White, Right - 134.0f, 31.0f);
    }

    if (!Character->IsInventoryOpen())
    {
        const bool bRifle = Character->HasRifle() && Character->IsRifleSelected();
        const bool bPistol = Character->HasSidearm() &&
            Character->GetHotbarItem(Character->GetActiveHotbarSlot()) == ESurvivalItem::Sidearm;
        const auto Held = Character->GetHotbarItem(Character->GetActiveHotbarSlot());
        if (IsFieldWeapon(Held))
        {
            const auto Ammo = Held == ESurvivalItem::Crossbow ? ESurvivalItem::Bolts : ESurvivalItem::Arrows;
            const FString Status = !Character->GetWeaponAssembly(Held).IsComplete() ? FString(TEXT("Parts missing")) :
                Held == ESurvivalItem::Knife ? FString(TEXT("LMB  ·  Strike")) :
                FString::Printf(TEXT("%d %s  ·  %s"), Character->GetItemCount(Ammo), SurvivalItemName(Ammo),
                    Held == ESurvivalItem::Crossbow ? TEXT("LMB to fire") : TEXT("Hold LMB to draw"));
            DrawText(Status, FLinearColor::White, Right - 215.f, Canvas->SizeY - 50.f);
        }
        if (bRifle || bPistol)
        {
            const bool bReloading = bRifle ? Character->IsRifleReloading() : Character->IsReloading();
            DrawRect(Panel, Right - 145.0f, Canvas->SizeY - 73.0f, 145.0f, 55.0f);
            DrawText(Character->GetInstanceLabel(Held, Character->GetToolbeltWeaponId(Character->GetActiveHotbarSlot())), Muted, Right - 132.0f, Canvas->SizeY - 66.0f);
            if (!Character->GetWeaponAssembly(bRifle ? ESurvivalItem::Rifle : ESurvivalItem::Sidearm).IsComplete())
                DrawText(TEXT("Parts missing"), Muted, Right - 132.0f, Canvas->SizeY - 45.0f);
            else DrawText(FString::Printf(TEXT("%d / %d%s"), bRifle ? Character->GetRifleAmmo() : Character->GetAmmo(),
                bRifle ? Character->GetRifleReserve() : Character->GetReserveAmmo(),
                bReloading ? TEXT("   Reloading") : TEXT("")), FLinearColor::White, Right - 132.0f, Canvas->SizeY - 45.0f);
        }
    }

    if (Character->IsDevSpeedActive())
    {
        DrawRect(Panel, Right - 196.0f, 65.0f, 196.0f, 25.0f);
        DrawText(TEXT("Dev speed 100 m/s [Caps Lock]"), Cyan, Right - 186.0f, 70.0f);
    }

    const int32 NightAlive = Character->GetLivingNightHorde();
    if (NightAlive > 0)
    {
        const FString DefenseStatus = FString::Printf(TEXT("Horde night   %d remaining"), NightAlive);
        float Width = 0.0f;
        float Height = 0.0f;
        GetTextSize(DefenseStatus, Width, Height);
        DrawRect(Panel, 20.0f, 65.0f, Width + 20.0f, Height + 10.0f);
        DrawText(DefenseStatus, Muted, 30.0f, 70.0f);
    }
    if (Character->GetHealthPoints() <= 0)
    {
        DrawText(TEXT("Downed   [F5] Restart"), Blocked, CenterX + 14.0f, CenterY + 12.0f);
        return;
    }

    if (const float Seconds = Character->GetSearchSecondsLeft(); Seconds > 0.0f)
    {
        DrawText(FString::Printf(TEXT("Searching   %.1f s"), Seconds),
            FLinearColor::White, CenterX - 55.0f, CenterY + 55.0f);
    }
    if (Character->IsInventoryOpen() || Character->HasOpenContainer())
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
    if (const ASurvivalWorldDoor* Door = Character->FindWorldDoor())
    {
        DrawText(Door->IsOpen() ? TEXT("[E] Close door") : TEXT("[E] Open door"),
            Cyan, CenterX + 14.0f, CenterY + 12.0f);
    }
    else if (const FSurvivalBuildPlacement* Plan = Target.GetPlan())
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
        const TCHAR* Status = !Crate->HasBeenSearched() ? TEXT(" (untouched)") :
            (Crate->HasSupplies() ? TEXT("") : TEXT(" (empty)"));
        const TCHAR* Action = Crate->IsDepleted() ? TEXT("") : TEXT("Press E to search ");
        DrawText(FString::Printf(TEXT("%s%s%s"), Action, *Crate->GetLootName(), Status),
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
        else if (!Character->IsBuilding() && Piece->Placement.GetPiece().bDoorFrame)
        {
            const bool bHasDoor = Character->GetItemCount(ESurvivalItem::TimberDoor) > 0;
            DrawText(bHasDoor ? TEXT("[E] Install timber door") : TEXT("Craft a timber door in the pack"),
                bHasDoor ? Cyan : Muted, CenterX + 14.0f, CenterY + 32.0f);
        }
        else if (Piece->Health < Piece->MaxHealth)
        {
            const FSurvivalMaterials Cost = Piece->RepairCost();
            const bool bCanRepair = Cost.MissingFrom(Inventory).IsEmpty();
            const FString Prompt = bCanRepair ? FString::Printf(TEXT("[E] Repair for %s"), *Cost.ToText())
                : FString::Printf(TEXT("Repair needs %s"), *Cost.ToText());
            const bool bToolBonus = bCanRepair && Character->GetItemCount(ESurvivalItem::SalvageTools) > 0;
            DrawText(bToolBonus ? Prompt + TEXT("  (+50% with tools)") : Prompt, bCanRepair ? Cyan : Blocked, CenterX + 14.0f, CenterY + 32.0f);
        }
    }
}

void ASurvivalHUD::DrawBuildMenu(const ASurvivalCharacter& Character)
{
    const TConstArrayView<int32> Pieces = GetPlayerBuildPieces();
    const float X = 20.0f;
    const float Y = 106.0f;
    constexpr float Width = 298.0f;
    constexpr float Row = 24.0f;
    const float Footer = Y + 42.0f + Row * Pieces.Num();
    DrawRect(Panel, X, Y, Width, Footer - Y + 49.0f);
    DrawText(TEXT("BUILD PLAN"), Cyan, X + 12.0f, Y + 12.0f, nullptr, 1.25f);
    for (int32 Index = 0; Index < Pieces.Num(); ++Index)
    {
        const FSurvivalBuildPiece& Piece = GetSurvivalBuildPieces()[Pieces[Index]];
        const float Top = Y + 40.0f + Row * Index;
        const bool bSelected = Pieces[Index] == Character.GetSelectedPiece();
        if (bSelected)
        {
            DrawRect(FLinearColor(0.10f, 0.23f, 0.29f, 0.95f), X + 7.0f, Top, Width - 14.0f, Row);
            DrawRect(Cyan, X + 7.0f, Top, 2.0f, Row);
        }
        DrawText(Piece.Name, bSelected ? FLinearColor::White : Muted, X + 15.0f, Top + 4.0f);
        DrawText(Piece.Cost.ToText(), bSelected ? Alloy : Muted, X + 193.0f, Top + 4.0f);
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
            const FVector Point = Placement.MeshOrigin() + Placement.Rotation.RotateVector(Offset);
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
