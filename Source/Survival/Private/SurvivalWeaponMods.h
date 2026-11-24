#pragma once
#include "CoreMinimal.h"

enum class EWeaponFamily : uint8 { Sidearm=1, Rifle=2, Melee=4, Bow=8 };
struct FWeaponModDefinition
{
    const TCHAR* Name;
    uint8 Families;
    FVector Dimensions;
    double AlongWeapon;
    double Height;
    float RecoilMultiplier;
};
inline const FWeaponModDefinition WeaponMods[] = {
    {TEXT("Grip wrap"), 15, {5,4,8}, -.22, -.24, .90f},
    {TEXT("Barrel weight"), 3, {9,4,4}, .78, .22, .85f}
};
