#include "SurvivalVitals.h"
#include "SurvivalWeaponMods.h"

const TCHAR* SurvivalItemName(ESurvivalItem Item)
{
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
    static const FSurvivalRecipe Recipes[] = {
        {ESurvivalItem::PrimitiveBow, TEXT("Weapons"), TEXT("A simple bow. Uses arrows and needs no separate parts."),
            {{ESurvivalItem::Timber, 4}, {ESurvivalItem::Cloth, 2}}},
        {ESurvivalItem::Knife, TEXT("Weapons"), TEXT("A one-piece field knife. Melee Weapon Crafting sets its quality."),
            {{ESurvivalItem::Stone, 3}, {ESurvivalItem::Cloth, 1}}},
        {ESurvivalItem::Arrows, TEXT("Ammunition"), TEXT("Ammunition for handheld bows."), {{ESurvivalItem::Timber, 1}}},
        {ESurvivalItem::Bolts, TEXT("Ammunition"), TEXT("Ammunition for crossbows."), {{ESurvivalItem::Timber, 1}, {ESurvivalItem::Stone, 1}}},
        {ESurvivalItem::Bandage, TEXT("Medical"), TEXT("Treats injuries. Medicine skill improves healing."), {{ESurvivalItem::Cloth, 2}}},
        {ESurvivalItem::TimberDoor, TEXT("Building"), TEXT("Install into a completed door frame."),
            {{ESurvivalItem::Timber, 4}}},
        {ESurvivalItem::FieldDressing, TEXT("Medical"), TEXT("A heavy dressing for treating deeper injuries."),
            {{ESurvivalItem::Bandage, 1}, {ESurvivalItem::Cloth, 2}}, ETraining::FieldDressings},
    };
    return MakeArrayView(Recipes);
}

void FSurvivalItems::Add(const FSurvivalItems& Other)
{
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
