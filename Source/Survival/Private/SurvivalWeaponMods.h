#pragma once
#include "SurvivalArsenal.h"

enum class EWeaponMount : uint8 { Optic, Muzzle, Laser, Magazine, Light, Underbarrel, Sling, Action, Stock, Binding, Handle, Balance };
inline constexpr int32 MountQuality[] = {1, 100, 200, 300, 400, 500, 600, 725, 850, 1, 1, 200};
inline constexpr int32 RequiredMountQuality(ESurvivalItem Weapon, int32 Mount)
{
    if (Mount == static_cast<int32>(EWeaponMount::Binding) &&
        (Weapon == ESurvivalItem::CompoundBow || Weapon == ESurvivalItem::Crossbow)) return 150;
    return MountQuality[Mount];
}

struct FWeaponModDefinition
{
    const TCHAR* Name;
    FVector Dimensions;
    float RecoilMultiplier;
    EWeaponMount Slot;
};
inline const FWeaponModDefinition WeaponMods[] = {
    {TEXT("Grip wrap"), {5,4,8}, .90f, EWeaponMount::Underbarrel},
    {TEXT("Barrel weight"), {9,4,4}, .85f, EWeaponMount::Muzzle},
    {TEXT("IR sight"), {8,3,4}, 1.0f, EWeaponMount::Optic},
    {TEXT("Variable zoom scope"), {16,3,4}, 1.0f, EWeaponMount::Optic},
    {TEXT("Laser"), {6,3,4}, 1.0f, EWeaponMount::Laser},
    {TEXT("Red laser"), {6,3,4}, 1.0f, EWeaponMount::Laser},
    {TEXT("IR laser"), {6,3,4}, 1.0f, EWeaponMount::Laser},
    {TEXT("Bipod"), {11,3,4}, .85f, EWeaponMount::Underbarrel},
    {TEXT("Suppressor"), {14,3,4}, 1.0f, EWeaponMount::Muzzle},
    {TEXT("Muzzle brake"), {5,3,4}, .85f, EWeaponMount::Muzzle},
    {TEXT("Extended magazine"), {8,3,4}, 1.0f, EWeaponMount::Magazine},
    {TEXT("Quick magazine"), {6,3,4}, 1.0f, EWeaponMount::Magazine},
    {TEXT("Sling"), {22,3,4}, 1.0f, EWeaponMount::Sling},
    {TEXT("Faster bolt"), {6,3,4}, 1.0f, EWeaponMount::Action},
    {TEXT("Flashlight"), {8,3,4}, 1.0f, EWeaponMount::Light},
    {TEXT("IR illuminator"), {8,3,4}, 1.0f, EWeaponMount::Light},
    {TEXT("Light stock"), {14,3,5}, .98f, EWeaponMount::Stock},
    {TEXT("CQB stock"), {10,4,6}, .94f, EWeaponMount::Stock},
    {TEXT("Heavy stock"), {18,5,7}, .80f, EWeaponMount::Stock},
    {TEXT("DRS-TH thermal sight"), {10,4,5}, 1.f, EWeaponMount::Optic},
    {TEXT("Factory rifle scope"), {18,4,4}, 1.f, EWeaponMount::Optic},
    {TEXT("Compact reflex sight"), {4,3,3}, 1.f, EWeaponMount::Optic},
    {TEXT("Burst trigger module"), {}, 1.f, EWeaponMount::Action},
    {TEXT("Automatic trigger module"), {}, 1.f, EWeaponMount::Action},
    {TEXT("Bowstring serving"), {}, 1.f, EWeaponMount::Binding},
    {TEXT("Archery reflex sight"), {5,3,4}, 1.f, EWeaponMount::Optic},
    {TEXT("Knife handle wrap"), {}, 1.f, EWeaponMount::Handle},
    {TEXT("Knife balance weight"), {}, 1.f, EWeaponMount::Balance},
};
enum EWeaponModIndex { GripWrap, BarrelWeight, IRSight, VariableScope, GreenLaser, RedLaser, IRLaser,
    Bipod, Suppressor, MuzzleBrake, ExtendedMagazine, QuickMagazine, Sling, FasterBolt, Flashlight, IRIlluminator,
    LightStock, CloseQuartersStock, HeavyStock, ThermalSight, FactoryScope, ReflexSight, BurstTrigger, AutoTrigger, BowBinding, BowSight, KnifeWrap, KnifeBalance, WeaponModCount };
static_assert(WeaponModCount == UE_ARRAY_COUNT(WeaponMods) && WeaponModCount <= 32);

inline constexpr ESurvivalItem AttachmentItem(int32 Mod)
{
    return static_cast<ESurvivalItem>(static_cast<int32>(ESurvivalItem::GripWrap) + Mod + (Mod >= BowBinding ? 1 : 0));
}
inline constexpr int32 AttachmentIndex(ESurvivalItem Item)
{
    return static_cast<int32>(Item) - static_cast<int32>(ESurvivalItem::GripWrap)
        - (Item >= ESurvivalItem::BowBinding ? 1 : 0);
}
static_assert(AttachmentItem(KnifeBalance) == ESurvivalItem::KnifeBalance);

inline bool FirearmAccepts(int32 Model, int32 Mod)
{
    if (Model < 0 || Model >= UE_ARRAY_COUNT(Arsenal) || Mod < 0 || Mod >= WeaponModCount) return false;
    const ESurvivalItem Family = Arsenal[Model].Family;
    if (IsFieldWeapon(Family))
    {
        switch (Family)
        {
        case ESurvivalItem::WoodenBow: return Mod == BowBinding;
        case ESurvivalItem::CompoundBow: return Mod == BowBinding || Mod == BowSight;
        case ESurvivalItem::Crossbow: return Mod == BowBinding || Mod == ReflexSight || Mod == VariableScope;
        case ESurvivalItem::Knife: return Mod == KnifeWrap || Mod == KnifeBalance;
        default: return false;
        }
    }
    if (Mod >= BowBinding) return false;
    const bool Pistol = Arsenal[Model].Family == ESurvivalItem::Sidearm;
    const bool LongRifle = Model == 1 || Model == 4 || Model == 5 || Model == 6;
    switch (Mod)
    {
    case BurstTrigger: case AutoTrigger: return Model == 0 || Model == 2;
    case FasterBolt: return Model == 1 || Model == 4;
    case FactoryScope: return Model == 1;
    case VariableScope: case Bipod: case ThermalSight: case IRSight: return LongRifle;
    case ExtendedMagazine: case QuickMagazine: case Suppressor: return Model != 3;
    case Sling: case HeavyStock: case CloseQuartersStock: case LightStock: return !Pistol;
    case GripWrap: case BarrelWeight: return true;
    default: return Model != 2 && Model != 3 && Model != 4;
    }
}
