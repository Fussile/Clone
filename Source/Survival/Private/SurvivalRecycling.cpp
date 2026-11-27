#include "SurvivalCharacter.h"
#include "SurvivalGear.h"
#include "SurvivalWeaponMods.h"

namespace
{
using ScrapItem = ESurvivalItem;

TArray<FSurvivalIngredient> GarmentMaterials(int32 Gear)
{
    using I = ScrapItem;
    if (Gear < 0 || Gear >= UE_ARRAY_COUNT(SurvivalGear)) return {};
    const auto& Def = SurvivalGear[Gear];
    const FString Slot(Def.Slot);
    const FString Id(Def.Id);
    if (Id == TEXT("woven_fiber_shirt")) return {{I::PlantFiber,8}};
    if (Id == TEXT("woven_fiber_trousers")) return {{I::PlantFiber,10}};
    if (Id.Contains(TEXT("ushanka"))) return {{I::Cloth,5}};
    if (Slot == TEXT("Vest")) return {{I::Cloth,7},{I::ScrapIron,5},{I::Plastic,2}};
    if (Slot == TEXT("Helmet"))
    {
        if (Id.Contains(TEXT("soviet_helmet"))) return {{I::ScrapIron,8},{I::Cloth,2}};
        return {{I::Plastic,7},{I::Cloth,2}};
    }
    if (Slot == TEXT("Face") || Slot == TEXT("Eyes")) return {{I::Plastic,5},{I::ScrapIron,2}};
    return {{I::Cloth, Slot == TEXT("Body") ? 10 : Slot == TEXT("Hands") ? 3 : 6}};
}

TArray<FSurvivalIngredient> ItemMaterials(ScrapItem Item)
{
    using I = ScrapItem;
    // Consumables and intermediate materials are excluded from the equipment scrap action.
    if (Item == I::TimberDoor || Item == I::PrimitiveBow || Item == I::Knife ||
        (Item >= I::SalvageTools && Item <= I::FilterPump) || (Item >= I::Hammer && Item <= I::PowerSupply) ||
        Item == I::Gear || Item == I::MachineSpring)
        for (const auto& Recipe : GetSurvivalRecipes())
            if (Recipe.Output == Item) return Recipe.Ingredients;

    if (IsAttachmentItem(Item))
    {
        if (Item == I::GripWrap || Item == I::KnifeWrap || Item == I::Sling || Item == I::BowBinding) return {{I::Cloth,5}};
        return {{I::ScrapIron,4},{I::Plastic,2}};
    }
    if (SurvivalItemHasQuality(Item))
    {
        if (Item == I::WoodenStave || Item == I::CrossbowStock || Item == I::RifleStock) return {{I::Timber,5}};
        if (Item == I::WoodenString || Item == I::CrossbowString) return {{I::PlantFiber,5}};
        return {{I::ScrapIron,5}};
    }
    if (IsWeaponItem(Item)) return {{I::ScrapIron,3},{I::Plastic,2}};
    return {};
}
}

bool ASurvivalCharacter::CanRecycle() const
{
    if (!bInventoryOpen || HealthPoints <= 0 || !Pack.IsValidIndex(SelectedPackSlot)) return false;
    const auto& Cell = Pack[SelectedPackSlot];
    if (Cell.IsEmpty()) return false;
    if (Cell.GearIndex != INDEX_NONE)
        return !bFieldRequestPending && GearCount(Cell.GearIndex) > (IsWearing(Cell.GearIndex) ? 1 : 0);
    return !ItemMaterials(Cell.Item).IsEmpty();
}

void ASurvivalCharacter::Recycle()
{
    if (!CanRecycle()) return;
    auto& Cell = Pack[SelectedPackSlot];
    if (Cell.GearIndex != INDEX_NONE)
    {
        bFieldRequestPending = true;
        ServerRecycleGear(Cell.GearIndex);
        return;
    }

    const auto Credit = [this](const TArray<FSurvivalIngredient>& Materials)
    {
        for (const auto& Ingredient : Materials)
            ScrapRemainders.FindOrAdd(Ingredient.Item) += Ingredient.Count;
    };
    if (IsWeaponItem(Cell.Item))
    {
        if (!WeaponItems.IsValidIndex(Cell.WeaponId)) return;
        auto& Instance = WeaponItems[Cell.WeaponId];
        if (Instance.Item != Cell.Item) return;
        for (int32 Slot = 0; Slot < FSurvivalWeaponAssembly::SlotsFor(Cell.Item); ++Slot)
            if (Instance.Assembly.GetPartQuality(Slot))
                Credit(ItemMaterials(static_cast<ScrapItem>(static_cast<int32>(FSurvivalWeaponAssembly::FirstPart(Cell.Item))+Slot)));
        for (int32 Mod = 0; Mod < WeaponModCount; ++Mod)
            if (Instance.Mods & (1u << Mod)) Credit(ItemMaterials(AttachmentItem(Mod)));
        if (Instance.Ammo && (Cell.Item == ScrapItem::Sidearm || Cell.Item == ScrapItem::Rifle))
            ScrapRemainders.FindOrAdd(Cell.Item == ScrapItem::Sidearm ? ScrapItem::Rounds : ScrapItem::RifleRounds) += 5 * Instance.Ammo;
        // Stable IDs are shared by pack cells, belt slots and the weapon inspector.
        Instance.Item = ScrapItem::None;
        Instance.Ammo = 0;
        Instance.Mods = 0;
    }
    else if (!IsAttachmentItem(Cell.Item) && !SurvivalItemHasQuality(Cell.Item))
        ChangeItemCount(Cell.Item, -1);

    Credit(ItemMaterials(Cell.Item));
    if (--Cell.Count == 0) Cell = {};
    SetSelectedInventoryItem(ScrapItem::None);
    SyncPack();
    FieldMessage(TEXT("Scrapped one item. Fractional recovery carries over."));
}

void ASurvivalCharacter::ServerRecycleGear_Implementation(int32 Gear)
{
    if (HealthPoints <= 0 || Gear < 0 || Gear >= UE_ARRAY_COUNT(SurvivalGear) ||
        GearCount(Gear) <= (IsWearing(Gear) ? 1 : 0))
    {
        ClientFieldResult(false, static_cast<uint8>(ScrapItem::None));
        return;
    }
    OwnedGear.RemoveSingle(Gear);
    ForceNetUpdate();
    ClientGearRecycled(Gear);
}

void ASurvivalCharacter::ClientGearRecycled_Implementation(int32 Gear)
{
    for (const auto& Ingredient : GarmentMaterials(Gear))
        ScrapRemainders.FindOrAdd(Ingredient.Item) += Ingredient.Count;
    bFieldRequestPending = false;
    SetSelectedInventoryItem(ScrapItem::None);
    SyncPack();
    FieldMessage(TEXT("Garment scrapped. Fractional recovery carries over."));
}
