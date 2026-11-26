#pragma once
#include "SurvivalVitals.h"

struct FSurvivalFirearm
{
    ESurvivalItem Family;
    const TCHAR* Asset;
    const TCHAR* Label;
    int32 Capacity;
};

inline constexpr FSurvivalFirearm Arsenal[] = {
    {ESurvivalItem::Sidearm, TEXT(""), TEXT("Sidearm"), 12},
    {ESurvivalItem::Rifle, TEXT(""), TEXT("Sniper rifle"), 4},
    {ESurvivalItem::Sidearm, TEXT("Makarov"), TEXT("Makarov PM"), 8},
    {ESurvivalItem::Sidearm, TEXT("Python357"), TEXT(".357 Python"), 6},
    {ESurvivalItem::Rifle, TEXT("ScrapRifle"), TEXT("Scrap rifle"), 4},
    {ESurvivalItem::Rifle, TEXT("AK12"), TEXT("AK-12"), 30},
    {ESurvivalItem::Rifle, TEXT("HK416"), TEXT("HK416 A7"), 30},
    {ESurvivalItem::Rifle, TEXT("MP7"), TEXT("MP7A1"), 20},
    {ESurvivalItem::Rifle, TEXT("Scorpion"), TEXT("Scorpion EVO 3"), 30},
};

inline int32 FirearmIndex(ESurvivalItem Family, int32 Index)
{
    return Index >= 0 && Index < UE_ARRAY_COUNT(Arsenal) && Arsenal[Index].Family == Family
        ? Index : (Family == ESurvivalItem::Rifle ? 1 : 0);
}
