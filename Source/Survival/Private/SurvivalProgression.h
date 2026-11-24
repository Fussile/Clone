#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"

enum class ETraining : uint8
{
    Athletics, Conditioning, Recovery, Scavenging, QuickSearch,
    LootAppraisal, TreasureTracking, ArmorUse, BluntWeapons, BluntTechnique,
    Breaching, BladeWeapons, BladeTechnique, HeavyBladeTechnique, Mining,
    EfficientMining, Construction, Workbench, ConcreteMixing, Steelworking,
    ChemistryStation, Salvage, Pistols, PistolTechnique, PistolPrecision,
    Shotguns, ShotgunTechnique, ShotgunPrecision, Rifles, RifleTechnique,
    Archery, Medicine, WeaponCraft, HeavyBladeCraft, ToolCraft,
    FirearmCraft, PistolAmmo, MagnumAmmo, ShotgunAmmo, RifleAmmo,
    AppliedScience, ElectricalBasics, ElectricalControls, ContactTraps, RangedTraps,
    Generators, BatteryStorage, PartsFabrication, ArmorCraft, BasicCraft,
    DecorativePaint, FauxPaint, MasonryPaint, MetalPaint, WallCovering,
    WoodFinish, Nutrition, Hydration, CraftSpeed, Repair,
    Barter, TradeAccess, FieldDressings, Count
};

struct FTrainingInfo
{
    const TCHAR* Name;
    const TCHAR* Category;
    const TCHAR* Icon;
    const TCHAR* Description;
    int32 InitialRank;
    int32 MaxRank;
    int32 BasePracticeXP;
    int32 BasePointCost;
    float PointCostMultiplier;
    bool bAvailable;

    bool IsPractice() const { return BasePracticeXP > 0; }
    int32 PointCost(int32 Rank) const;
};

struct FTrainingGate
{
    ETraining Entry;
    int32 Rank;
    ETraining RequiredEntry;
    int32 RequiredRank;
};

const FTrainingInfo& GetTrainingInfo(ETraining Entry);
TConstArrayView<FTrainingGate> GetTrainingGates();
float TrainingEffectAtRank(ETraining Entry, int32 Rank);

struct FSurvivalProgression
{
    static constexpr int32 EntryCount = static_cast<int32>(ETraining::Count);
    static constexpr int32 PointsPerLevel = 6;
    static constexpr int32 LevelCap = 200;

    int32 Points = 0;
    int32 CharacterLevel = 1;

    int32 Rank(ETraining Entry) const;
    int32 Practice(ETraining Entry) const;
    int32 PracticeRequired(ETraining Entry) const;
    float Effect(ETraining Entry) const { return TrainingEffectAtRank(Entry, Rank(Entry)); }
    int32 CharacterXP() const { return LevelProgress; }
    int32 LevelXPRequired() const { return 100 + 25 * (CharacterLevel - 1); }
    void AwardPractice(ETraining Entry, int32 Amount);
    void AwardXP(int32 Amount);
    bool CanReadGuide(ETraining Entry) const;
    void ReadGuide(ETraining Entry);
    int32 PointCostThrough(ETraining Entry, int32 TargetRank) const;
    bool CanBuyThrough(ETraining Entry, int32 TargetRank) const;
    bool BuyThrough(ETraining Entry, int32 TargetRank);

private:
    int32 SkillProgress[EntryCount]{};
    int32 EarnedRanks[EntryCount]{};
    int32 LevelProgress = 0;
};
