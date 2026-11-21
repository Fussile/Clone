#pragma once

#include "CoreMinimal.h"

enum class ESurvivalSkill : uint8 { Marksmanship, Building, Endurance, Scavenging, Count };
enum class ESurvivalPerk : uint8 { SteadyHand, SoundRepairs, SecondWind, StockFinder, Workshop, Count };

struct FSurvivalSkillInfo
{
    const TCHAR* Name;
    const TCHAR* Practice;
    const TCHAR* Icon;
};

struct FSurvivalPerkInfo
{
    const TCHAR* Name;
    const TCHAR* Effect;
    const TCHAR* Icon;
    ESurvivalSkill Skill;
    int32 Ranks;
    int32 AmountPerRank;
    int32 FirstCost;
    int32 CostStep;
    int32 LevelStep;
    int32 SkillStep;

    int32 Cost(int32 Rank) const { return FirstCost + (Rank - 1) * CostStep; }
    int32 RequiredLevel(int32 Rank) const { return 1 + (Rank - 1) * LevelStep; }
    int32 RequiredSkill(int32 Rank) const { return 1 + Rank * SkillStep; }
};

const FSurvivalSkillInfo& GetSurvivalSkillInfo(ESurvivalSkill Skill);
const FSurvivalPerkInfo& GetSurvivalPerkInfo(ESurvivalPerk Perk);

struct FSurvivalProgression
{
    static constexpr int32 SkillCount = static_cast<int32>(ESurvivalSkill::Count);
    static constexpr int32 PerkCount = static_cast<int32>(ESurvivalPerk::Count);
    static constexpr int32 MaxRank = 100;
    static constexpr int32 PointsPerLevel = 6;

    int32 Points = 0;
    int32 CharacterLevel = 1;

    int32 Rank(ESurvivalSkill Skill) const { return SkillRanks[static_cast<int32>(Skill)] + 1; }
    int32 Practice(ESurvivalSkill Skill) const;
    int32 PracticeRequired(ESurvivalSkill Skill) const { return 26 + 4 * Rank(Skill); }
    int32 CharacterXP() const { return LevelProgress; }
    int32 LevelXPRequired() const { return 100 + 25 * (CharacterLevel - 1); }
    void AwardPractice(ESurvivalSkill Skill, int32 Amount);
    void AwardXP(int32 Amount);
    void ReadGuide(ESurvivalSkill Skill);
    int32 PerkRank(ESurvivalPerk Perk) const { return Perks[static_cast<int32>(Perk)]; }
    int32 Bonus(ESurvivalPerk Perk) const;
    int32 PerkCostThrough(ESurvivalPerk Perk, int32 TargetRank) const;
    bool CanBuyPerkThrough(ESurvivalPerk Perk, int32 TargetRank) const;
    bool BuyPerk(ESurvivalPerk Perk);

private:
    int32 SkillProgress[SkillCount]{};
    int32 SkillRanks[SkillCount]{};
    int32 Perks[PerkCount]{};
    int32 LevelProgress = 0;
};
