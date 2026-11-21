#pragma once

#include "CoreMinimal.h"

enum class ESurvivalSkill : uint8 { Marksmanship, Building, Endurance, Count };

struct FSurvivalSkillInfo
{
    const TCHAR* Name;
    const TCHAR* Perk;
    const TCHAR* Benefit;
};

const FSurvivalSkillInfo& GetSurvivalSkillInfo(ESurvivalSkill Skill);

struct FSurvivalProgression
{
    static constexpr int32 PerkRank = 3;
    static constexpr int32 SkillCount = static_cast<int32>(ESurvivalSkill::Count);
    static constexpr int32 PracticePerRank = 30;
    static constexpr int32 MaxRank = 100;

    int32 Points = 0;
    int32 CharacterLevel = 1;
    int32 Rank(ESurvivalSkill Skill) const;
    int32 Practice(ESurvivalSkill Skill) const;
    void AwardPractice(ESurvivalSkill Skill, int32 Amount);
    void ReadGuide(ESurvivalSkill Skill);
    bool HasPerk(ESurvivalSkill Skill) const;
    bool CanBuyPerk(ESurvivalSkill Skill) const;
    bool BuyPerk(ESurvivalSkill Skill);

private:
    void AddSkillXP(ESurvivalSkill Skill, int32 Amount);

    int32 SkillXP[SkillCount]{};
    bool Perks[SkillCount]{};
    int32 LevelProgress = 0;
};
