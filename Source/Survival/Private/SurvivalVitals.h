#pragma once

#include "CoreMinimal.h"

enum class ESurvivalItem : uint8
{
    Sidearm, Food, Water, Bandage, Timber, Stone, Cloth, Rounds, None
};

const TCHAR* SurvivalItemName(ESurvivalItem Item);

struct FSurvivalItems
{
    int32 Food = 0;
    int32 Water = 0;
    int32 Cloth = 0;
    int32 Bandages = 0;
    int32 Rounds = 0;

    static constexpr int32 BandageCloth = 2;

    void Add(const FSurvivalItems& Other);
    bool CraftBandage();
};

struct FSurvivalVitals
{
    static constexpr float Max = 100.0f;

    float Hunger = Max;
    float Thirst = Max;
    float Stamina = Max;

    bool CanSprint() const { return !bExhausted; }
    int32 Advance(float DeltaSeconds, bool bSprinting, float MovementEffort, float AmbientCelsius);

private:
    float DeprivationTime = 0.0f;
    bool bExhausted = false;
};
