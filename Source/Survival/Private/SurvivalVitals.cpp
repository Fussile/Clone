#include "SurvivalVitals.h"
#include "SurvivalWeaponMods.h"

namespace
{
struct FWorkshopItem
{
    const TCHAR* Name;
    int32 StackSize;
};

constexpr FWorkshopItem WorkshopItems[] = {
    {TEXT("Iron ore"), 100},
    {TEXT("Copper ore"), 100},
    {TEXT("Scrap iron"), 100},
    {TEXT("Salvaged tin"), 100},
    {TEXT("Iron ingot"), 50},
    {TEXT("Copper ingot"), 50},
    {TEXT("Bronze ingot"), 50},
    {TEXT("Machine spring"), 50},
    {TEXT("Gear"), 50},
    {TEXT("Salvage tool roll"), 1},
    {TEXT("Field crucible"), 1},
    {TEXT("Hand-crank filter pump"), 1},
    {TEXT("Empty bottle"), 20},
    {TEXT("Untreated water"), 20},
    {TEXT("Cloth-strained water"), 20},
    {TEXT("Plant fibers"), 100},
    {TEXT("Scrap plastic"), 100},
    {TEXT("Forged steel"), 50},
    {TEXT("Primitive building hammer"), 1},
    {TEXT("Stone axe"), 1},
    {TEXT("Stone spear"), 1},
    {TEXT("Bedroll"), 1},
    {TEXT("Woven fiber shirt"), 1},
    {TEXT("Woven fiber trousers"), 1},
    {TEXT("Chemistry set"), 1},
    {TEXT("Machining bench"), 1},
    {TEXT("Forge"), 1},
    {TEXT("Campfire"), 1},
    {TEXT("Electric stove"), 1},
    {TEXT("3D printer"), 1},
    {TEXT("Catalyst"), 1},
    {TEXT("Decanter"), 1},
    {TEXT("Pressure chamber"), 1},
    {TEXT("Lathe"), 1},
    {TEXT("Sawmill"), 1},
    {TEXT("Mill"), 1},
    {TEXT("Leaf blower bellows"), 1},
    {TEXT("Casting mold"), 1},
    {TEXT("Cooking pot"), 1},
    {TEXT("Car radiator grill"), 1},
    {TEXT("Oven"), 1},
    {TEXT("Efficient power supply"), 1},
    {TEXT("Standard filament"), 50},
    {TEXT("Reinforced filament"), 50},
    {TEXT("Power cell"), 20},
    {TEXT("Grilled meal"), 20},
    {TEXT("Baked meal"), 20},
    {TEXT("Machining manual"), 3},
    {TEXT("Chemistry manual"), 3},
    {TEXT("Steelworking manual"), 3},
};
static_assert(UE_ARRAY_COUNT(WorkshopItems) == WorkshopIndex(ESurvivalItem::Count));
}

const TCHAR* SurvivalItemName(ESurvivalItem Item)
{
    if (IsWorkshopItem(Item)) return WorkshopItems[WorkshopIndex(Item)].Name;
    if (IsAttachmentItem(Item)) return WeaponMods[AttachmentIndex(Item)].Name;
    switch (Item)
    {
    case ESurvivalItem::Sidearm: return TEXT("Sidearm");
    case ESurvivalItem::Food: return TEXT("Canned food");
    case ESurvivalItem::Water: return TEXT("Bottled water");
    case ESurvivalItem::Bandage: return TEXT("Bandage");
    case ESurvivalItem::Timber: return TEXT("Timber");
    case ESurvivalItem::Stone: return TEXT("Stone");
    case ESurvivalItem::Cloth: return TEXT("Cloth");
    case ESurvivalItem::Rounds: return TEXT("Pistol rounds");
    case ESurvivalItem::TimberDoor: return TEXT("Timber door");
    case ESurvivalItem::Rifle: return TEXT("Bolt-action rifle");
    case ESurvivalItem::RifleRounds: return TEXT("Rifle rounds");
    case ESurvivalItem::BuilderGuide: return TEXT("Builder's guide");
    case ESurvivalItem::ScavengerNotes: return TEXT("Scavenger's field notes");
    case ESurvivalItem::DressingSchematic: return TEXT("Field dressing schematic");
    case ESurvivalItem::FieldDressing: return TEXT("Field dressing");
    case ESurvivalItem::RifleStock: return TEXT("Rifle stock");
    case ESurvivalItem::RifleReceiver: return TEXT("Rifle receiver");
    case ESurvivalItem::RifleBarrel: return TEXT("Rifle barrel");
    case ESurvivalItem::RifleMagazine: return TEXT("Rifle magazine");
    case ESurvivalItem::SidearmGrip: return TEXT("Sidearm grip");
    case ESurvivalItem::SidearmFrame: return TEXT("Sidearm frame");
    case ESurvivalItem::SidearmBarrel: return TEXT("Sidearm barrel");
    case ESurvivalItem::SidearmSlide: return TEXT("Sidearm slide");
    case ESurvivalItem::PrimitiveBow: return TEXT("Primitive bow");
    case ESurvivalItem::WoodenBow: return TEXT("Wooden bow");
    case ESurvivalItem::CompoundBow: return TEXT("Compound bow");
    case ESurvivalItem::Crossbow: return TEXT("Crossbow");
    case ESurvivalItem::Knife: return TEXT("Field knife");
    case ESurvivalItem::Arrows: return TEXT("Arrows");
    case ESurvivalItem::Bolts: return TEXT("Crossbow bolts");
    case ESurvivalItem::WoodenStave: return TEXT("Wooden bow stave");
    case ESurvivalItem::WoodenString: return TEXT("Wooden bowstring");
    case ESurvivalItem::CompoundRiser: return TEXT("Compound riser");
    case ESurvivalItem::CompoundLimbs: return TEXT("Compound limb set");
    case ESurvivalItem::CompoundCams: return TEXT("Compound cam set");
    case ESurvivalItem::CrossbowStock: return TEXT("Crossbow stock");
    case ESurvivalItem::CrossbowProd: return TEXT("Crossbow limb assembly");
    case ESurvivalItem::CrossbowString: return TEXT("Crossbow string");
    case ESurvivalItem::CrossbowLatch: return TEXT("Crossbow latch");
    default: return TEXT("Empty");
    }
}

const TCHAR* FSurvivalWeaponAssembly::PartName(int32 Slot, ESurvivalItem Weapon)
{
    if (Slot < 0 || Slot >= SlotsFor(Weapon)) return TEXT("");
    return SurvivalItemName(static_cast<ESurvivalItem>(static_cast<int32>(FirstPart(Weapon)) + Slot));
}

int32 FSurvivalWeaponAssembly::SpareQuality(int32 Slot)
{
    constexpr int32 Quality[] = {72, 84, 76, 80};
    static_assert(UE_ARRAY_COUNT(Quality) == PartCount);
    return Quality[Slot];
}

void FSurvivalWeaponAssembly::Initialize(ESurvivalItem Weapon, int32 Quality)
{
    ActiveSlots = SlotsFor(Weapon);
    BodyQuality = FMath::Clamp(Quality, 1, MaxQuality);
    for (int32 Slot = 0; Slot < PartCount; ++Slot)
        Parts[Slot] = Slot < ActiveSlots ? BodyQuality : 0;
}

bool FSurvivalWeaponAssembly::IsComplete() const
{
    for (int32 Slot = 0; Slot < ActiveSlots; ++Slot)
        if (Parts[Slot] == 0) return false;
    return true;
}

int32 FSurvivalWeaponAssembly::GetQuality() const
{
    if (ActiveSlots == 0) return BodyQuality;
    int32 Sum = 0;
    for (int32 Slot = 0; Slot < ActiveSlots; ++Slot)
    {
        Sum += Parts[Slot];
    }
    return FMath::RoundToInt(static_cast<float>(Sum) / ActiveSlots);
}

float FSurvivalWeaponAssembly::GetDamage() const
{
    if (!IsComplete()) return 0.0f;
    const int32 Quality = GetQuality();
    if (Quality <= FactoryQuality) return 70.0f + (Quality - FactoryQuality) * 0.5f;
    return 70.0f + (Quality - FactoryQuality) * (30.0f / (MaxQuality - FactoryQuality));
}

int32 FSurvivalWeaponAssembly::SwapPart(int32 Slot, int32 NewQuality)
{
    if (Slot < 0 || Slot >= ActiveSlots)
    {
        return NewQuality;
    }
    const int32 Replaced = Parts[Slot];
    Parts[Slot] = FMath::Clamp(NewQuality, 0, MaxQuality);
    return Replaced;
}

// Provisional stack limits. Tune freely: the backpack lays stacks out from these.
int32 SurvivalItemMaxStack(ESurvivalItem Item)
{
    if (IsWorkshopItem(Item)) return WorkshopItems[WorkshopIndex(Item)].StackSize;
    switch (Item)
    {
    case ESurvivalItem::Food:
    case ESurvivalItem::Water: return 20;
    case ESurvivalItem::Bandage:
    case ESurvivalItem::FieldDressing: return 10;
    case ESurvivalItem::Timber:
    case ESurvivalItem::Stone: return 500;
    case ESurvivalItem::Cloth: return 100;
    case ESurvivalItem::Arrows: case ESurvivalItem::Bolts: return 50;
    case ESurvivalItem::Rounds:
    case ESurvivalItem::RifleRounds: return 200;
    case ESurvivalItem::TimberDoor:
    case ESurvivalItem::BuilderGuide:
    case ESurvivalItem::ScavengerNotes:
    case ESurvivalItem::DressingSchematic: return 5;
    default: return 1;
    }
}

TConstArrayView<FSurvivalRecipe> GetSurvivalRecipes()
{
    using Item = ESurvivalItem;
    static const FSurvivalRecipe Recipes[] = {
        {ESurvivalItem::PrimitiveBow, TEXT("Weapons"), TEXT("A simple bow. Uses arrows and needs no separate parts."),
            {{ESurvivalItem::Timber, 4}, {ESurvivalItem::PlantFiber, 2}}},
        {ESurvivalItem::Knife, TEXT("Weapons"), TEXT("A one-piece field knife. Melee Weapon Crafting sets its quality."),
            {{ESurvivalItem::Stone, 3}, {ESurvivalItem::Cloth, 1}}},
        {ESurvivalItem::Arrows, TEXT("Ammunition"), TEXT("Ammunition for handheld bows."), {{ESurvivalItem::Timber, 1}, {ESurvivalItem::Plastic, 1}}},
        {ESurvivalItem::Bolts, TEXT("Ammunition"), TEXT("Ammunition for crossbows."), {{ESurvivalItem::Timber, 1}, {ESurvivalItem::Stone, 1}}},
        {ESurvivalItem::Bandage, TEXT("Medical"), TEXT("Treats injuries. Medicine skill improves healing."), {{ESurvivalItem::Cloth, 2}}},
        {ESurvivalItem::TimberDoor, TEXT("Building"), TEXT("Install into a completed door frame."),
            {{ESurvivalItem::Timber, 4}}},
        {ESurvivalItem::FieldDressing, TEXT("Medical"), TEXT("A heavy dressing for treating deeper injuries."),
            {{ESurvivalItem::Bandage, 1}, {ESurvivalItem::Cloth, 2}}, ETraining::FieldDressings},
        {Item::SalvageTools, TEXT("Tools"), TEXT("Work reclaimed metal and assemble mechanisms. Carrying the roll also restores 50% more structural health per repair."),
            {{Item::ScrapIron, 3}, {Item::Timber, 2}, {Item::Cloth, 2}}},
        {Item::FieldCrucible, TEXT("Tools"), TEXT("Install in a forge to refine ore."),
            {{Item::Stone, 8}, {Item::ScrapIron, 3}, {Item::Cloth, 1}}, ETraining::Count, Item::SalvageTools},
        {Item::IronIngot, TEXT("Metals"), TEXT("Refine the iron-bearing host rock found around geodes."),
            {{Item::IronOre, 2}, {Item::Timber, 1}}, ETraining::Count, Item::None, Item::Forge, Item::FieldCrucible},
        {Item::CopperIngot, TEXT("Metals"), TEXT("Refine copper-bearing rock into a workable ingot."),
            {{Item::CopperOre, 2}, {Item::Timber, 1}}, ETraining::Count, Item::None, Item::Forge, Item::FieldCrucible},
        {Item::BronzeIngot, TEXT("Metals"), TEXT("Cast a bronze blank for a pump housing."),
            {{Item::CopperIngot, 1}, {Item::TinScrap, 1}, {Item::Timber, 1}}, ETraining::Count, Item::None, Item::Forge, Item::FieldCrucible},
        {Item::MachineSpring, TEXT("Components"), TEXT("Rework scrap into a replacement machine spring."),
            {{Item::ScrapIron, 2}}, ETraining::Workbench, Item::None, Item::MachineBench, Item::Lathe},
        {Item::Gear, TEXT("Components"), TEXT("Shape an iron gear for a hand-driven mechanism."),
            {{Item::IronIngot, 1}}, ETraining::Workbench, Item::None, Item::MachineBench, Item::Mill},
        {Item::FilterPump, TEXT("Tools"), TEXT("A reusable hand pump for filtering bottled river or lake water."),
            {{Item::BronzeIngot, 1}, {Item::Gear, 1}, {Item::MachineSpring, 1}, {Item::Cloth, 3}},
            ETraining::Count, Item::SalvageTools},
        {Item::StrainedWater, TEXT("Water"), TEXT("Strain a bottle through cloth. Drinking it still carries a high illness risk."),
            {{Item::RawWater, 1}, {Item::Cloth, 1}}},
        {Item::StrainedWater, TEXT("Water"), TEXT("Reuse the filter pump to strain water without spending fresh cloth."),
            {{Item::RawWater, 1}}, ETraining::Count, Item::FilterPump},
        {Item::Water, TEXT("Water"), TEXT("Purify a bottle using the campfire."),
            {{Item::RawWater, 1}, {Item::Timber, 1}}, ETraining::Count, Item::None, Item::Campfire, Item::Pot},
        {Item::Water, TEXT("Water"), TEXT("Purify a bottle using the campfire."),
            {{Item::StrainedWater, 1}, {Item::Timber, 1}}, ETraining::Count, Item::None, Item::Campfire, Item::Pot},
        {Item::Water, TEXT("Water"), TEXT("Purify a bottle using the chemistry set."),
            {{Item::RawWater, 1}, {Item::Timber, 1}}, ETraining::Count, Item::Pot, Item::ChemSet},
        {Item::Water, TEXT("Water"), TEXT("Purify a bottle using the chemistry set."),
            {{Item::StrainedWater, 1}, {Item::Timber, 1}}, ETraining::Count, Item::Pot, Item::ChemSet},
        {Item::Water, TEXT("Water"), TEXT("Purify a bottle using the electric stove."),
            {{Item::RawWater, 1}, {Item::FuelCell, 1}}, ETraining::Count, Item::None, Item::Stove, Item::Pot},
        {Item::Water, TEXT("Water"), TEXT("Purify a bottle using the electric stove."),
            {{Item::StrainedWater, 1}, {Item::FuelCell, 1}}, ETraining::Count, Item::None, Item::Stove, Item::Pot},
        {Item::Hammer, TEXT("Fieldcraft"), TEXT("Equip and right-click a player-built frame or block to upgrade it."),
            {{Item::Timber, 2}, {Item::Stone, 2}, {Item::PlantFiber, 2}}},
        {Item::StoneAxe, TEXT("Fieldcraft"), TEXT("Harvest more timber from tree trunks."),
            {{Item::Timber, 2}, {Item::Stone, 3}, {Item::PlantFiber, 2}}},
        {Item::StoneSpear, TEXT("Fieldcraft"), TEXT("A close-range field weapon."),
            {{Item::Timber, 3}, {Item::Stone, 2}, {Item::PlantFiber, 2}}},
        {Item::Bedroll, TEXT("Fieldcraft"), TEXT("Place on level ground and interact to rest."),
            {{Item::PlantFiber, 8}, {Item::Cloth, 2}}},
        {Item::FiberShirt, TEXT("Fieldcraft"), TEXT("Wear as a torso garment."),
            {{Item::PlantFiber, 8}}},
        {Item::FiberPants, TEXT("Fieldcraft"), TEXT("Wear as a leg garment."),
            {{Item::PlantFiber, 10}}},
        {Item::ChemSet, TEXT("Stations"), TEXT("Place on level ground, then interact to use its recipes and install equipment."),
            {{Item::ScrapIron, 8}, {Item::EmptyBottle, 4}, {Item::Plastic, 5}}},
        {Item::MachineBench, TEXT("Stations"), TEXT("Place on level ground, then interact to use its recipes and install equipment."),
            {{Item::Timber, 12}, {Item::ScrapIron, 10}}},
        {Item::Forge, TEXT("Stations"), TEXT("Place on level ground, then interact to use its recipes and install equipment."),
            {{Item::Stone, 15}, {Item::ScrapIron, 5}}},
        {Item::Campfire, TEXT("Stations"), TEXT("Place on level ground, then interact to use its recipes and install equipment."),
            {{Item::Stone, 4}, {Item::Timber, 3}}},
        {Item::Stove, TEXT("Stations"), TEXT("Place on level ground, then interact to use its recipes and install equipment."),
            {{Item::ScrapIron, 8}, {Item::CopperIngot, 2}}},
        {Item::Printer, TEXT("Stations"), TEXT("Place on level ground, then interact to use its recipes and install equipment."),
            {{Item::Plastic, 12}, {Item::CopperIngot, 4}, {Item::Gear, 2}}},
        {Item::Pot, TEXT("Expansions"), TEXT("Install at the campfire or electric stove."),
            {{Item::ScrapIron, 3}}},
        {Item::Catalyst, TEXT("Expansions"), TEXT("Install at the chemistry set."),
            {{Item::CopperOre, 2}, {Item::TinScrap, 2}}},
        {Item::Decanter, TEXT("Expansions"), TEXT("Install at the chemistry set."),
            {{Item::EmptyBottle, 2}, {Item::Plastic, 2}}},
        {Item::Chamber, TEXT("Expansions"), TEXT("Install at the chemistry set."),
            {{Item::ScrapIron, 5}, {Item::CopperIngot, 1}}},
        {Item::Lathe, TEXT("Expansions"), TEXT("Install at the machining bench."),
            {{Item::ScrapIron, 6}, {Item::Timber, 3}}},
        {Item::Sawmill, TEXT("Expansions"), TEXT("Install at the machining bench."),
            {{Item::ScrapIron, 5}, {Item::Timber, 4}}},
        {Item::Mill, TEXT("Expansions"), TEXT("Install at the machining bench."),
            {{Item::ScrapIron, 8}, {Item::Timber, 3}}},
        {Item::Bellows, TEXT("Expansions"), TEXT("Install at the forge."),
            {{Item::Plastic, 4}, {Item::ScrapIron, 3}}},
        {Item::Mold, TEXT("Expansions"), TEXT("Install at the forge (reserved for future casting recipes)."),
            {{Item::Stone, 4}, {Item::ScrapIron, 2}}},
        {Item::Grill, TEXT("Expansions"), TEXT("Install at the campfire."),
            {{Item::ScrapIron, 4}, {Item::TinScrap, 2}}},
        {Item::Oven, TEXT("Expansions"), TEXT("Install at the electric stove."),
            {{Item::ScrapIron, 6}, {Item::TinScrap, 3}}},
        {Item::PowerSupply, TEXT("Expansions"), TEXT("Install at the electric stove."),
            {{Item::CopperIngot, 2}, {Item::ScrapIron, 3}}},
        {Item::Steel, TEXT("Metals"), TEXT("Refine forged steel for the last construction tier."),
            {{Item::IronIngot, 3}, {Item::Timber, 2}}, ETraining::Steelworking, Item::None, Item::Forge, Item::FieldCrucible},
        {Item::Filament, TEXT("Components"), TEXT("Make a spool of standard printer filament."),
            {{Item::Plastic, 3}}, ETraining::Count, Item::None, Item::ChemSet, Item::Decanter},
        {Item::ToughFilament, TEXT("Components"), TEXT("Produce reinforced filament."),
            {{Item::Filament, 1}, {Item::PlantFiber, 2}}, ETraining::ChemistryStation, Item::None, Item::ChemSet, Item::Chamber},
        {Item::FieldDressing, TEXT("Medical"), TEXT("Use the catalyst to make a dressing with less cloth."),
            {{Item::Bandage, 1}, {Item::Cloth, 1}}, ETraining::FieldDressings, Item::None, Item::ChemSet, Item::Catalyst},
        {Item::TimberDoor, TEXT("Building"), TEXT("Cut a door with less timber."),
            {{Item::Timber, 2}}, ETraining::Count, Item::None, Item::MachineBench, Item::Sawmill},
        {Item::Gear, TEXT("Components"), TEXT("Print a replacement gear with reinforced filament."),
            {{Item::ToughFilament, 1}, {Item::FuelCell, 1}}, ETraining::Workbench, Item::None, Item::Printer},
        {Item::EmptyBottle, TEXT("Water"), TEXT("Print an empty bottle."),
            {{Item::Filament, 1}, {Item::FuelCell, 1}}, ETraining::Count, Item::None, Item::Printer},
        {Item::FuelCell, TEXT("Components"), TEXT("Assemble a portable power cell."),
            {{Item::CopperOre, 1}, {Item::ScrapIron, 1}, {Item::Plastic, 1}}},
        {Item::GrilledMeal, TEXT("Food"), TEXT("Prepare a filling meal on a radiator grill."),
            {{Item::Food, 1}, {Item::Timber, 1}}, ETraining::Count, Item::None, Item::Campfire, Item::Grill},
        {Item::OvenMeal, TEXT("Food"), TEXT("Bake a filling meal."),
            {{Item::Food, 1}, {Item::FuelCell, 1}}, ETraining::Count, Item::None, Item::Stove, Item::Oven},
    };
    return MakeArrayView(Recipes);
}

void FSurvivalItems::Add(const FSurvivalItems& Other)
{
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(WorkshopStacks); ++Index)
        WorkshopStacks[Index] += Other.WorkshopStacks[Index];
    Food += Other.Food;
    Water += Other.Water;
    Cloth += Other.Cloth;
    Bandages += Other.Bandages;
    Rounds += Other.Rounds;
    TimberDoors += Other.TimberDoors;
    RifleRounds += Other.RifleRounds;
    Arrows += Other.Arrows;
    Bolts += Other.Bolts;
    BuilderGuides += Other.BuilderGuides;
    ScavengerNotes += Other.ScavengerNotes;
    DressingSchematics += Other.DressingSchematics;
    FieldDressings += Other.FieldDressings;
    for (int32 Slot = 0; Slot < FSurvivalWeaponAssembly::PartCount; ++Slot)
    {
        RifleParts[Slot] += Other.RifleParts[Slot];
        SidearmParts[Slot] += Other.SidearmParts[Slot];
    }
}

int32 FSurvivalVitals::Advance(float DeltaSeconds, bool bSprinting, float MovementEffort, float AmbientCelsius,
    float SprintCost, float StaminaRecovery, float HungerDrain, float ThirstDrain)
{
    const float ColdDegrees = FMath::Max(0.0f, 12.0f - AmbientCelsius);
    const float CalorieUse = 0.013f + 0.043f * FMath::Square(MovementEffort) + 0.0035f * ColdDegrees;
    const float Before = Stamina;
    Stamina = FMath::Clamp(Stamina + DeltaSeconds * (bSprinting ? -18.0f * SprintCost : 13.0f * StaminaRecovery), 0.0f, Max);
    const float StaminaSpent = FMath::Max(0.0f, Before - Stamina);

    Hunger = FMath::Max(0.0f, Hunger - DeltaSeconds * CalorieUse * HungerDrain);
    Thirst = FMath::Max(0.0f, Thirst - (DeltaSeconds * 0.022f + StaminaSpent * 0.009f) * ThirstDrain);
    if (Stamina <= 0.0f)
    {
        bExhausted = true;
    }
    else if (Stamina >= 20.0f)
    {
        bExhausted = false;
    }

    const int32 EmptyMeters = (Hunger <= 0.0f ? 1 : 0) + (Thirst <= 0.0f ? 1 : 0);
    if (EmptyMeters == 0)
    {
        DeprivationTime = 0.0f;
        return 0;
    }
    DeprivationTime += DeltaSeconds;
    const int32 DamageTicks = FMath::FloorToInt(DeprivationTime);
    DeprivationTime -= DamageTicks;
    return DamageTicks * EmptyMeters;
}
