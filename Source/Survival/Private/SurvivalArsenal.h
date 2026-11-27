#pragma once
#include "SurvivalVitals.h"

struct FSurvivalFirearm
{
    ESurvivalItem Family;
    const TCHAR* Asset;
    const TCHAR* Label;
    int32 Capacity;
    float AutomaticInterval = 0.f;
};

inline constexpr FSurvivalFirearm Arsenal[] = {
    {ESurvivalItem::Sidearm, TEXT(""), TEXT("Sidearm"), 12},
    {ESurvivalItem::Rifle, TEXT(""), TEXT("Sniper rifle"), 4},
    {ESurvivalItem::Sidearm, TEXT("Makarov"), TEXT("Makarov PM"), 8},
    {ESurvivalItem::Sidearm, TEXT("Python357"), TEXT(".357 Python"), 6},
    {ESurvivalItem::Rifle, TEXT("ScrapRifle"), TEXT("Scrap rifle"), 4},
    {ESurvivalItem::Rifle, TEXT("AK12"), TEXT("AK-12"), 30, .12f},
    {ESurvivalItem::Rifle, TEXT("HK416"), TEXT("HK416 A7"), 30, .11f},
    {ESurvivalItem::Rifle, TEXT("MP7"), TEXT("MP7A1"), 20, .085f},
    {ESurvivalItem::Rifle, TEXT("Scorpion"), TEXT("Scorpion EVO 3"), 30, .095f},
    {ESurvivalItem::PrimitiveBow, TEXT("PrimitiveBow"), TEXT("Primitive bow"), 0},
    {ESurvivalItem::WoodenBow, TEXT("WoodenBow"), TEXT("Wooden bow"), 0},
    {ESurvivalItem::CompoundBow, TEXT("CompoundBow"), TEXT("Compound bow"), 0},
    {ESurvivalItem::Crossbow, TEXT("Crossbow"), TEXT("Crossbow"), 0},
    {ESurvivalItem::Knife, TEXT("Knife"), TEXT("Field knife"), 0},
};

inline int32 FirearmIndex(ESurvivalItem Family, int32 Index)
{
    if (Index >= 0 && Index < UE_ARRAY_COUNT(Arsenal) && Arsenal[Index].Family == Family) return Index;
    for (int32 Candidate = 0; Candidate < UE_ARRAY_COUNT(Arsenal); ++Candidate)
        if (Arsenal[Candidate].Family == Family) return Candidate;
    return INDEX_NONE;
}
