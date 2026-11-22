#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "SurvivalProgression.h"

enum class ESurvivalItem : uint8
{
    Sidearm, Food, Water, Bandage, Timber, Stone, Cloth, Rounds, TimberDoor, Rifle, RifleRounds, BuilderGuide, ScavengerNotes, DressingSchematic, FieldDressing,
    RifleStock, RifleReceiver, RifleBarrel, RifleOptic, None
};

const TCHAR* SurvivalItemName(ESurvivalItem Item);

struct FSurvivalRifle
{
    static constexpr int32 PartCount = 4;
    static constexpr int32 FactoryQuality = 40;
    static constexpr int32 PartIndex(ESurvivalItem Item)
    {
        const int32 Slot = static_cast<int32>(Item) - static_cast<int32>(ESurvivalItem::RifleStock);
        return Slot >= 0 && Slot < PartCount ? Slot : INDEX_NONE;
    }

    static const TCHAR* PartName(int32 Slot);
    static int32 SpareQuality(int32 Slot);
    int32 GetPartQuality(int32 Slot) const { return Parts[Slot]; }
    int32 GetQuality() const;
    float GetDamage() const;
    // Installs a part of the given quality and returns the quality of the part it replaced.
    int32 SwapPart(int32 Slot, int32 NewQuality);

private:
    int32 Parts[PartCount] = {FactoryQuality, FactoryQuality, FactoryQuality, FactoryQuality};
};
static_assert(FSurvivalRifle::PartIndex(ESurvivalItem::RifleOptic) == FSurvivalRifle::PartCount - 1);

// One backpack cell. Plain items stack up to SurvivalItemMaxStack. Parts carry their own quality and never stack.
struct FSurvivalPackCell
{
    ESurvivalItem Item = ESurvivalItem::None;
    int32 Count = 0;
    int32 Quality = 0;

    bool IsEmpty() const { return Item == ESurvivalItem::None || Count <= 0; }
};

int32 SurvivalItemMaxStack(ESurvivalItem Item);
inline bool SurvivalItemHasQuality(ESurvivalItem Item) { return FSurvivalRifle::PartIndex(Item) != INDEX_NONE; }

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
    ETraining RequiredTraining = ETraining::Count;
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
    int32 ScavengerNotes = 0;
    int32 DressingSchematics = 0;
    int32 FieldDressings = 0;
    int32 RifleParts[FSurvivalRifle::PartCount]{};

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
