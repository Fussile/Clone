#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"

enum class ESurvivalItem : uint8
{
    Sidearm, Food, Water, Bandage, Timber, Stone, Cloth, Rounds, TimberDoor, Rifle, RifleRounds, BuilderGuide, None
};

const TCHAR* SurvivalItemName(ESurvivalItem Item);

struct FSurvivalIngredient
{
    ESurvivalItem Item;
    int32 Count;
};

struct FSurvivalRecipe
{
    ESurvivalItem Output;
    const TCHAR* Category;
    const TCHAR* Description;
    TArray<FSurvivalIngredient> Ingredients;
};

TConstArrayView<FSurvivalRecipe> GetSurvivalRecipes();

struct FSurvivalItems
{
    int32 Food = 0;
    int32 Water = 0;
    int32 Cloth = 0;
    int32 Bandages = 0;
    int32 Rounds = 0;

    int32 TimberDoors = 0;
    int32 RifleRounds = 0;
    int32 BuilderGuides = 0;

    void Add(const FSurvivalItems& Other);
};

struct FSurvivalVitals
{
    static constexpr float Max = 100.0f;

    float Hunger = Max;
    float Thirst = Max;
    float Stamina = Max;

    bool CanSprint() const { return !bExhausted; }
    int32 Advance(float DeltaSeconds, bool bSprinting, float MovementEffort, float AmbientCelsius, float SprintCost, float StaminaRecovery, float HungerDrain, float ThirstDrain);

private:
    float DeprivationTime = 0.0f;
    bool bExhausted = false;
};
