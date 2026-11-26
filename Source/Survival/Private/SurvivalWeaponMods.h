#pragma once
#include "SurvivalArsenal.h"

enum class EWeaponMount : uint8 { Optic, Muzzle, Laser, Magazine, Light, Underbarrel, Sling, Action, Stock };
inline constexpr int32 MountQuality[] = {1, 100, 200, 300, 400, 500, 600, 725, 850};
struct FWeaponModDefinition
{
    const TCHAR* Name;
    FVector Dimensions;
    double AlongWeapon;
    double Height;
    float RecoilMultiplier;
    EWeaponMount Slot;
};
inline const FWeaponModDefinition WeaponMods[] = {
    {TEXT("Grip wrap"), {5,4,8}, -.22, -.24, .90f, EWeaponMount::Underbarrel},
    {TEXT("Barrel weight"), {9,4,4}, .78, .22, .85f, EWeaponMount::Muzzle},
    {TEXT("IR sight"), {8,3,4}, 0, 1, 1.0f, EWeaponMount::Optic},
    {TEXT("Variable zoom scope"), {16,3,4}, 0, 1, 1.0f, EWeaponMount::Optic},
    {TEXT("Laser"), {6,3,4}, 0.5, 0.25, 1.0f, EWeaponMount::Laser},
    {TEXT("Red laser"), {6,3,4}, 0.5, 0.25, 1.0f, EWeaponMount::Laser},
    {TEXT("IR laser"), {6,3,4}, 0.5, 0.25, 1.0f, EWeaponMount::Laser},
    {TEXT("Bipod"), {11,3,4}, 0.55, -0.8, .85f, EWeaponMount::Underbarrel},
    {TEXT("Suppressor"), {14,3,4}, 1.15, 0, 1.0f, EWeaponMount::Muzzle},
    {TEXT("Muzzle brake"), {5,3,4}, 1.05, 0, .85f, EWeaponMount::Muzzle},
    {TEXT("Extended magazine"), {8,3,4}, -0.1, -1, 1.0f, EWeaponMount::Magazine},
    {TEXT("Quick magazine"), {6,3,4}, -0.1, -0.7, 1.0f, EWeaponMount::Magazine},
    {TEXT("Sling"), {22,3,4}, -0.35, -0.55, 1.0f, EWeaponMount::Sling},
    {TEXT("Faster bolt"), {6,3,4}, -0.3, 0.2, 1.0f, EWeaponMount::Action},
    {TEXT("Flashlight"), {8,3,4}, 0.6, 0.1, 1.0f, EWeaponMount::Light},
    {TEXT("IR illuminator"), {8,3,4}, 0.6, 0.1, 1.0f, EWeaponMount::Light},
    {TEXT("Light stock"), {14,3,5}, -.8, -.1, .98f, EWeaponMount::Stock},
    {TEXT("CQB stock"), {10,4,6}, -.8, -.1, .94f, EWeaponMount::Stock},
    {TEXT("Heavy stock"), {18,5,7}, -.8, -.1, .80f, EWeaponMount::Stock},
    {TEXT("DRS-TH thermal sight"), {10,4,5}, 0, 1, 1.f, EWeaponMount::Optic},
    {TEXT("Factory rifle scope"), {18,4,4}, 0, 1, 1.f, EWeaponMount::Optic},
    {TEXT("Compact reflex sight"), {4,3,3}, 0, 1, 1.f, EWeaponMount::Optic},
    {TEXT("Burst trigger module"), {}, 0, 0, 1.f, EWeaponMount::Action},
    {TEXT("Automatic trigger module"), {}, 0, 0, 1.f, EWeaponMount::Action},
};
enum EWeaponModIndex { GripWrap, BarrelWeight, IRSight, VariableScope, GreenLaser, RedLaser, IRLaser,
    Bipod, Suppressor, MuzzleBrake, ExtendedMagazine, QuickMagazine, Sling, FasterBolt, Flashlight, IRIlluminator,
    LightStock, CloseQuartersStock, HeavyStock, ThermalSight, FactoryScope, ReflexSight, BurstTrigger, AutoTrigger, WeaponModCount };
static_assert(WeaponModCount == UE_ARRAY_COUNT(WeaponMods) && WeaponModCount <= 32);

inline constexpr ESurvivalItem AttachmentItem(int32 Mod)
{
    return static_cast<ESurvivalItem>(static_cast<int32>(ESurvivalItem::GripWrap) + Mod);
}
static_assert(AttachmentItem(WeaponModCount) == ESurvivalItem::None);

inline bool FirearmAccepts(int32 Model, int32 Mod)
{
    if (Model < 0 || Model >= UE_ARRAY_COUNT(Arsenal) || Mod < 0 || Mod >= WeaponModCount) return false;
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
