#include "SurvivalProgression.h"

namespace
{
struct FTrainingEffect
{
    ETraining Entry;
    int32 FirstRank;
    int32 LastRank;
    float FirstValue;
    float LastValue;
};

#include "SurvivalTrainingCatalog.inl"

static_assert(UE_ARRAY_COUNT(Training) == FSurvivalProgression::EntryCount);
}

const FTrainingInfo& GetTrainingInfo(ETraining Entry)
{
    return Training[static_cast<int32>(Entry)];
}

TConstArrayView<FTrainingGate> GetTrainingGates()
{
    return MakeArrayView(Gates);
}

float TrainingEffectAtRank(ETraining Entry, int32 Rank)
{
    for (const FTrainingEffect& Effect : Effects)
    {
        if (Effect.Entry == Entry && Rank >= Effect.FirstRank && Rank <= Effect.LastRank)
        {
            const int32 Span = Effect.LastRank - Effect.FirstRank;
            return Span == 0 ? Effect.FirstValue : FMath::Lerp(Effect.FirstValue, Effect.LastValue,
                static_cast<float>(Rank - Effect.FirstRank) / Span);
        }
    }
    return 1.0f;
}

int32 FTrainingInfo::PointCost(int32 Rank) const
{
    const int32 Multiplier = PointCostMultiplier > 0.0f
        ? FMath::FloorToInt(FMath::Pow(PointCostMultiplier, Rank - 1)) : 1;
    return BasePointCost * Multiplier;
}

int32 FSurvivalProgression::Rank(ETraining Entry) const
{
    return GetTrainingInfo(Entry).InitialRank + EarnedRanks[static_cast<int32>(Entry)];
}

int32 FSurvivalProgression::Practice(ETraining Entry) const
{
    return Rank(Entry) == GetTrainingInfo(Entry).MaxRank ? PracticeRequired(Entry) : SkillProgress[static_cast<int32>(Entry)];
}

int32 FSurvivalProgression::PracticeRequired(ETraining Entry) const
{
    return FMath::RoundToInt(GetTrainingInfo(Entry).BasePracticeXP *
        FMath::Pow(1.03f, EarnedRanks[static_cast<int32>(Entry)]));
}

void FSurvivalProgression::AwardPractice(ETraining Entry, int32 Amount)
{
    const FTrainingInfo& Info = GetTrainingInfo(Entry);
    if (Amount <= 0 || !Info.IsPractice() || !Info.bAvailable || Rank(Entry) >= Info.MaxRank)
    {
        return;
    }
    const int32 Index = static_cast<int32>(Entry);
    SkillProgress[Index] += Amount;
    while (Rank(Entry) < Info.MaxRank && SkillProgress[Index] >= PracticeRequired(Entry))
    {
        SkillProgress[Index] -= PracticeRequired(Entry);
        ++EarnedRanks[Index];
    }
}

void FSurvivalProgression::AwardXP(int32 Amount)
{
    if (Amount <= 0 || CharacterLevel >= LevelCap)
    {
        return;
    }
    LevelProgress += Amount;
    while (CharacterLevel < LevelCap && LevelProgress >= LevelXPRequired())
    {
        LevelProgress -= LevelXPRequired();
        ++CharacterLevel;
        Points += PointsPerLevel;
    }
    if (CharacterLevel == LevelCap)
    {
        LevelProgress = LevelXPRequired();
    }
}

bool FSurvivalProgression::CanReadGuide(ETraining Entry) const
{
    const FTrainingInfo& Info = GetTrainingInfo(Entry);
    return Info.bAvailable && Rank(Entry) < Info.MaxRank;
}

void FSurvivalProgression::ReadGuide(ETraining Entry)
{
    if (CanReadGuide(Entry))
    {
        ++EarnedRanks[static_cast<int32>(Entry)];
    }
}

int32 FSurvivalProgression::PointCostThrough(ETraining Entry, int32 TargetRank) const
{
    const FTrainingInfo& Info = GetTrainingInfo(Entry);
    int32 Cost = 0;
    for (int32 Level = Rank(Entry) + 1; Level <= TargetRank && Level <= Info.MaxRank; ++Level)
    {
        Cost += Info.PointCost(Level);
    }
    return Cost;
}

bool FSurvivalProgression::CanBuyThrough(ETraining Entry, int32 TargetRank) const
{
    const FTrainingInfo& Info = GetTrainingInfo(Entry);
    const int32 CurrentRank = Rank(Entry);
    if (Info.IsPractice() || !Info.bAvailable || TargetRank <= CurrentRank || TargetRank > Info.MaxRank
        || Points < PointCostThrough(Entry, TargetRank))
    {
        return false;
    }
    for (const FTrainingGate& Gate : GetTrainingGates())
    {
        if (Gate.Entry == Entry && Gate.Rank <= TargetRank && Gate.Rank > CurrentRank)
        {
            const int32 ActualRank = Gate.RequiredEntry == ETraining::Count ? CharacterLevel : Rank(Gate.RequiredEntry);
            if (ActualRank < Gate.RequiredRank)
            {
                return false;
            }
        }
    }
    return true;
}

bool FSurvivalProgression::BuyThrough(ETraining Entry, int32 TargetRank)
{
    if (!CanBuyThrough(Entry, TargetRank))
    {
        return false;
    }
    Points -= PointCostThrough(Entry, TargetRank);
    EarnedRanks[static_cast<int32>(Entry)] = TargetRank - GetTrainingInfo(Entry).InitialRank;
    return true;
}
