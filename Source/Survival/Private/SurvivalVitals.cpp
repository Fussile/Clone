#include "SurvivalVitals.h"

void FSurvivalItems::Add(const FSurvivalItems& Other)
{
    Food += Other.Food;
    Water += Other.Water;
    Cloth += Other.Cloth;
    Bandages += Other.Bandages;
    Rounds += Other.Rounds;
}

bool FSurvivalItems::CraftBandage()
{
    if (Cloth < BandageCloth)
    {
        return false;
    }
    Cloth -= BandageCloth;
    ++Bandages;
    return true;
}

int32 FSurvivalVitals::Advance(float DeltaSeconds, bool bSprinting)
{
    Hunger = FMath::Max(0.0f, Hunger - DeltaSeconds * (bSprinting ? 0.22f : 0.10f));
    Thirst = FMath::Max(0.0f, Thirst - DeltaSeconds * (bSprinting ? 0.34f : 0.16f));
    Stamina = FMath::Clamp(Stamina + DeltaSeconds * (bSprinting ? -18.0f : 13.0f), 0.0f, Max);
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
