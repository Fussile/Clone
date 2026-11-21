#include "SurvivalVitals.h"

const TCHAR* SurvivalItemName(ESurvivalItem Item)
{
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
    default: return TEXT("Empty");
    }
}

TConstArrayView<FSurvivalRecipe> GetSurvivalRecipes()
{
    static const FSurvivalRecipe Recipes[] = {
        {ESurvivalItem::Bandage, TEXT("Medical"), TEXT("Restores 30 health."), {{ESurvivalItem::Cloth, 2}}},
        {ESurvivalItem::TimberDoor, TEXT("Building"), TEXT("Install into a completed door frame."),
            {{ESurvivalItem::Timber, 4}}},
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
    BuilderGuides += Other.BuilderGuides;
}

int32 FSurvivalVitals::Advance(float DeltaSeconds, bool bSprinting, float MovementEffort, float AmbientCelsius, float SprintCost)
{
    const float ColdDegrees = FMath::Max(0.0f, 12.0f - AmbientCelsius);
    const float CalorieUse = 0.013f + 0.043f * FMath::Square(MovementEffort) + 0.0035f * ColdDegrees;
    const float Before = Stamina;
    Stamina = FMath::Clamp(Stamina + DeltaSeconds * (bSprinting ? -18.0f * SprintCost : 13.0f), 0.0f, Max);
    const float StaminaSpent = FMath::Max(0.0f, Before - Stamina);

    Hunger = FMath::Max(0.0f, Hunger - DeltaSeconds * CalorieUse);
    Thirst = FMath::Max(0.0f, Thirst - (DeltaSeconds * 0.022f + StaminaSpent * 0.009f));
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
