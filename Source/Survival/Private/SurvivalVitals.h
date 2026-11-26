#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "SurvivalProgression.h"
#include "SurvivalVitals.generated.h"

enum class ESurvivalItem : uint8
{
    Sidearm, Food, Water, Bandage, Timber, Stone, Cloth, Rounds, TimberDoor, Rifle, RifleRounds, BuilderGuide, ScavengerNotes, DressingSchematic, FieldDressing,
    RifleStock, RifleReceiver, RifleBarrel, RifleMagazine, SidearmGrip, SidearmFrame, SidearmBarrel, SidearmSlide,
    GripWrap, BarrelWeight, IRSight, VariableScope, GreenLaser, RedLaser, IRLaser, Bipod,
    Suppressor, MuzzleBrake, ExtendedMagazine, QuickMagazine, Sling, FasterBolt, Flashlight, IRIlluminator,
    LightStock, CloseQuartersStock, HeavyStock, ThermalSight, FactoryScope, ReflexSight, BurstTrigger, AutoTrigger, None
};

inline constexpr bool IsAttachmentItem(ESurvivalItem Item)
{
    return Item >= ESurvivalItem::GripWrap && Item < ESurvivalItem::None;
}

const TCHAR* SurvivalItemName(ESurvivalItem Item);

struct FSurvivalWeaponAssembly
{
    static constexpr int32 PartCount = 4;
    static constexpr int32 FactoryQuality = 40;
    static constexpr int32 MaxQuality = 999;
    static constexpr ESurvivalItem FirstPart(ESurvivalItem Weapon)
    {
        return Weapon == ESurvivalItem::Sidearm ? ESurvivalItem::SidearmGrip : ESurvivalItem::RifleStock;
    }

    static constexpr int32 PartIndex(ESurvivalItem Item, ESurvivalItem Weapon = ESurvivalItem::Rifle)
    {
        const int32 Slot = static_cast<int32>(Item) - static_cast<int32>(FirstPart(Weapon));
        return Slot >= 0 && Slot < PartCount ? Slot : INDEX_NONE;
    }

    static const TCHAR* PartName(int32 Slot, ESurvivalItem Weapon = ESurvivalItem::Rifle);
    static int32 SpareQuality(int32 Slot);
    int32 GetPartQuality(int32 Slot) const { return Parts[Slot]; }
    bool IsComplete() const;
    int32 GetQuality() const;
    float GetDamage() const;
    // Installs a part of the given quality and returns the quality of the part it replaced.
    int32 SwapPart(int32 Slot, int32 NewQuality);

private:
    int32 Parts[PartCount] = {FactoryQuality, FactoryQuality, FactoryQuality, FactoryQuality};
};
static_assert(FSurvivalWeaponAssembly::PartIndex(ESurvivalItem::RifleMagazine) == FSurvivalWeaponAssembly::PartCount - 1);

// One backpack cell. Plain items stack up to SurvivalItemMaxStack. Parts carry their own quality and never stack.
struct FSurvivalPackCell
{
    ESurvivalItem Item = ESurvivalItem::None;
    int32 Count = 0;
    int32 Quality = 0;

    int32 GearIndex = INDEX_NONE;
    int32 WeaponId = INDEX_NONE;

    bool IsEmpty() const { return (Item == ESurvivalItem::None && GearIndex == INDEX_NONE) || Count <= 0; }
};

int32 SurvivalItemMaxStack(ESurvivalItem Item);
inline bool SurvivalItemHasQuality(ESurvivalItem Item)
{
    return FSurvivalWeaponAssembly::PartIndex(Item) != INDEX_NONE || FSurvivalWeaponAssembly::PartIndex(Item, ESurvivalItem::Sidearm) != INDEX_NONE;
}

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

USTRUCT()
struct FSurvivalItems
{
    GENERATED_BODY()

    UPROPERTY()
    int32 Food = 0;
    UPROPERTY()
    int32 Water = 0;
    UPROPERTY()
    int32 Cloth = 0;
    UPROPERTY()
    int32 Bandages = 0;
    UPROPERTY()
    int32 Rounds = 0;

    UPROPERTY()
    int32 TimberDoors = 0;
    UPROPERTY()
    int32 RifleRounds = 0;
    UPROPERTY()
    int32 BuilderGuides = 0;
    UPROPERTY()
    int32 ScavengerNotes = 0;
    UPROPERTY()
    int32 DressingSchematics = 0;
    UPROPERTY()
    int32 FieldDressings = 0;
    UPROPERTY()
    int32 RifleParts[4]{};
    UPROPERTY()
    int32 SidearmParts[4]{};

    static_assert(FSurvivalWeaponAssembly::PartCount == 4);
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
