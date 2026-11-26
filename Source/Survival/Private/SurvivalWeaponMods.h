#pragma once
#include "CoreMinimal.h"

enum class EWeaponFamily : uint8 { Sidearm=1, Rifle=2, Melee=4, Bow=8 };
enum class EWeaponMount : uint8 { Optic, Muzzle, Laser, Magazine, Light, Underbarrel, Sling, Action, Stock };
inline constexpr int32 MountQuality[] = {1, 100, 200, 300, 400, 500, 600, 725, 850};
struct FWeaponModDefinition
{
    const TCHAR* Name;
    uint8 Families;
    FVector Dimensions;
    double AlongWeapon;
    double Height;
    float RecoilMultiplier;
    EWeaponMount Slot;
};
inline const FWeaponModDefinition WeaponMods[] = {
    {TEXT("Grip wrap"), 15, {5,4,8}, -.22, -.24, .90f, EWeaponMount::Underbarrel},
    {TEXT("Barrel weight"), 3, {9,4,4}, .78, .22, .85f, EWeaponMount::Muzzle},
    {TEXT("IR sight"), 3, {8,3,4}, 0, 1, 1.0f, EWeaponMount::Optic},
    {TEXT("Variable zoom scope"), 3, {16,3,4}, 0, 1, 1.0f, EWeaponMount::Optic},
    {TEXT("Laser"), 3, {6,3,4}, 0.5, 0.25, 1.0f, EWeaponMount::Laser},
    {TEXT("Red laser"), 3, {6,3,4}, 0.5, 0.25, 1.0f, EWeaponMount::Laser},
    {TEXT("IR laser"), 3, {6,3,4}, 0.5, 0.25, 1.0f, EWeaponMount::Laser},
    {TEXT("Bipod"), 2, {11,3,4}, 0.55, -0.8, .85f, EWeaponMount::Underbarrel},
    {TEXT("Suppressor"), 3, {14,3,4}, 1.15, 0, 1.0f, EWeaponMount::Muzzle},
    {TEXT("Muzzle brake"), 3, {5,3,4}, 1.05, 0, .85f, EWeaponMount::Muzzle},
    {TEXT("Extended magazine"), 3, {8,3,4}, -0.1, -1, 1.0f, EWeaponMount::Magazine},
    {TEXT("Quick magazine"), 3, {6,3,4}, -0.1, -0.7, 1.0f, EWeaponMount::Magazine},
    {TEXT("Sling"), 3, {22,3,4}, -0.35, -0.55, 1.0f, EWeaponMount::Sling},
    {TEXT("Faster bolt"), 2, {6,3,4}, -0.3, 0.2, 1.0f, EWeaponMount::Action},
    {TEXT("Flashlight"), 3, {8,3,4}, 0.6, 0.1, 1.0f, EWeaponMount::Light},
    {TEXT("IR illuminator"), 3, {8,3,4}, 0.6, 0.1, 1.0f, EWeaponMount::Light},
    {TEXT("Light stock"), 2, {14,3,5}, -.8, -.1, .98f, EWeaponMount::Stock},
    {TEXT("CQB stock"), 2, {10,4,6}, -.8, -.1, .94f, EWeaponMount::Stock},
    {TEXT("Heavy stock"), 2, {18,5,7}, -.8, -.1, .80f, EWeaponMount::Stock},
    {TEXT("DRS-TH thermal sight"), 3, {10,4,5}, 0, 1, 1.f, EWeaponMount::Optic},
};
enum EWeaponModIndex { GripWrap, BarrelWeight, IRSight, VariableScope, GreenLaser, RedLaser, IRLaser,
    Bipod, Suppressor, MuzzleBrake, ExtendedMagazine, QuickMagazine, Sling, FasterBolt, Flashlight, IRIlluminator,
    LightStock, CloseQuartersStock, HeavyStock, ThermalSight, WeaponModCount };
static_assert(WeaponModCount == UE_ARRAY_COUNT(WeaponMods) && WeaponModCount <= 32);
