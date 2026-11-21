#include "SurvivalProgression.h"

const FSurvivalSkillInfo& GetSurvivalSkillInfo(ESurvivalSkill Skill)
{
    static const FSurvivalSkillInfo Skills[] = {
        {TEXT("Marksmanship"), TEXT("Land shots on infected to practice firearm handling."), TEXT("Combat")},
        {TEXT("Construction"), TEXT("Finish plans, install doors, and repair completed structures."), TEXT("Construction")},
        {TEXT("Athletics"), TEXT("Spend time sprinting to build endurance."), TEXT("Athletics")},
        {TEXT("Scavenging"), TEXT("Open unclaimed supply crates to gain experience."), TEXT("Scavenging")},
    };
    static_assert(UE_ARRAY_COUNT(Skills) == FSurvivalProgression::SkillCount);
    return Skills[static_cast<int32>(Skill)];
}

const FSurvivalPerkInfo& GetSurvivalPerkInfo(ESurvivalPerk Perk)
{
    static const FSurvivalPerkInfo Perks[] = {
        {TEXT("Steady Hand"), TEXT("Reduces firearm recoil by {0}%."), TEXT("Combat"),
            ESurvivalSkill::Marksmanship, 5, 5, 4, 1, 3, 2},
        {TEXT("Sound Repairs"), TEXT("Restores {0} additional structure health per repair."), TEXT("Construction"),
            ESurvivalSkill::Building, 5, 3, 4, 1, 3, 2},
        {TEXT("Second Wind"), TEXT("Reduces sprint stamina drain by {0}%."), TEXT("Athletics"),
            ESurvivalSkill::Endurance, 5, 4, 4, 1, 3, 2},
        {TEXT("Stock Finder"), TEXT("Recovers up to {0} additional units of each building material already in a cache."), TEXT("Scavenging"),
            ESurvivalSkill::Scavenging, 5, 1, 4, 1, 3, 2},
        {TEXT("Workshop Repair"), TEXT("Restores {0} additional structure health per repair."), TEXT("Workshop"),
            ESurvivalSkill::Count, 10, 2, 4, 1, 2, 0},
    };
    static_assert(UE_ARRAY_COUNT(Perks) == FSurvivalProgression::PerkCount);
    return Perks[static_cast<int32>(Perk)];
}

int32 FSurvivalProgression::Practice(ESurvivalSkill Skill) const
{
    return Rank(Skill) == MaxRank ? PracticeRequired(Skill) : SkillProgress[static_cast<int32>(Skill)];
}

void FSurvivalProgression::AwardPractice(ESurvivalSkill Skill, int32 Amount)
{
    if (Amount <= 0)
    {
        return;
    }
    const int32 Index = static_cast<int32>(Skill);
    if (Rank(Skill) < MaxRank)
    {
        SkillProgress[Index] += Amount;
        while (Rank(Skill) < MaxRank && SkillProgress[Index] >= PracticeRequired(Skill))
        {
            SkillProgress[Index] -= PracticeRequired(Skill);
            ++SkillRanks[Index];
        }
    }
    AwardXP(Amount);
}

void FSurvivalProgression::AwardXP(int32 Amount)
{
    LevelProgress += Amount;
    while (LevelProgress >= LevelXPRequired())
    {
        LevelProgress -= LevelXPRequired();
        ++CharacterLevel;
        Points += PointsPerLevel;
    }
}

void FSurvivalProgression::ReadGuide(ESurvivalSkill Skill)
{
    if (Rank(Skill) < MaxRank)
    {
        ++SkillRanks[static_cast<int32>(Skill)];
    }
}

int32 FSurvivalProgression::Bonus(ESurvivalPerk Perk) const
{
    return PerkRank(Perk) * GetSurvivalPerkInfo(Perk).AmountPerRank;
}

int32 FSurvivalProgression::PerkCostThrough(ESurvivalPerk Perk, int32 TargetRank) const
{
    const FSurvivalPerkInfo& Info = GetSurvivalPerkInfo(Perk);
    int32 Cost = 0;
    for (int32 Rank = PerkRank(Perk) + 1; Rank <= TargetRank && Rank <= Info.Ranks; ++Rank)
    {
        Cost += Info.Cost(Rank);
    }
    return Cost;
}

bool FSurvivalProgression::CanBuyPerkThrough(ESurvivalPerk Perk, int32 TargetRank) const
{
    const FSurvivalPerkInfo& Info = GetSurvivalPerkInfo(Perk);
    return TargetRank > PerkRank(Perk) && TargetRank <= Info.Ranks
        && Points >= PerkCostThrough(Perk, TargetRank) && CharacterLevel >= Info.RequiredLevel(TargetRank)
        && (Info.Skill == ESurvivalSkill::Count || Rank(Info.Skill) >= Info.RequiredSkill(TargetRank));
}

bool FSurvivalProgression::BuyPerk(ESurvivalPerk Perk)
{
    if (!CanBuyPerkThrough(Perk, PerkRank(Perk) + 1))
    {
        return false;
    }
    int32& PerkLevel = Perks[static_cast<int32>(Perk)];
    ++PerkLevel;
    Points -= GetSurvivalPerkInfo(Perk).Cost(PerkLevel);
    return true;
}
