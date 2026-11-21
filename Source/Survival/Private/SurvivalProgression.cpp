#include "SurvivalProgression.h"

const FSurvivalSkillInfo& GetSurvivalSkillInfo(ESurvivalSkill Skill)
{
    static const FSurvivalSkillInfo Skills[] = {
        {TEXT("Marksmanship"), TEXT("Steady Hand"), TEXT("25% less firearm recoil")},
        {TEXT("Building"), TEXT("Sound Repairs"), TEXT("Restore 15 more structure health per repair")},
        {TEXT("Endurance"), TEXT("Second Wind"), TEXT("20% lower stamina cost while sprinting")}
    };
    static_assert(UE_ARRAY_COUNT(Skills) == FSurvivalProgression::SkillCount);
    return Skills[static_cast<int32>(Skill)];
}

int32 FSurvivalProgression::Rank(ESurvivalSkill Skill) const
{
    return 1 + SkillXP[static_cast<int32>(Skill)] / PracticePerRank;
}

int32 FSurvivalProgression::Practice(ESurvivalSkill Skill) const
{
    return Rank(Skill) == MaxRank ? PracticePerRank : SkillXP[static_cast<int32>(Skill)] % PracticePerRank;
}

void FSurvivalProgression::AddSkillXP(ESurvivalSkill Skill, int32 Amount)
{
    int32& XP = SkillXP[static_cast<int32>(Skill)];
    XP = FMath::Min((MaxRank - 1) * PracticePerRank, XP + Amount);
}

void FSurvivalProgression::AwardPractice(ESurvivalSkill Skill, int32 Amount)
{
    if (Amount <= 0)
    {
        return;
    }
    AddSkillXP(Skill, Amount);
    constexpr int32 XPPerCharacterLevel = 100;
    LevelProgress += Amount;
    const int32 Levels = LevelProgress / XPPerCharacterLevel;
    LevelProgress %= XPPerCharacterLevel;
    CharacterLevel += Levels;
    Points += Levels;
}

void FSurvivalProgression::ReadGuide(ESurvivalSkill Skill)
{
    AddSkillXP(Skill, PracticePerRank);
}

bool FSurvivalProgression::HasPerk(ESurvivalSkill Skill) const
{
    return Perks[static_cast<int32>(Skill)];
}

bool FSurvivalProgression::CanBuyPerk(ESurvivalSkill Skill) const
{
    return Points > 0 && Rank(Skill) >= PerkRank && !HasPerk(Skill);
}

bool FSurvivalProgression::BuyPerk(ESurvivalSkill Skill)
{
    if (!CanBuyPerk(Skill))
    {
        return false;
    }
    --Points;
    Perks[static_cast<int32>(Skill)] = true;
    return true;
}
