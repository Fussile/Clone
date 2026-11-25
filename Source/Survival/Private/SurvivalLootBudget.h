#pragma once

#include "SurvivalSupplyCrate.h"

namespace SurvivalLootBudget
{
int32 GetGameStage(int32 Level, int32 DaysSurvived);
int32 GetUnitValue(const FSurvivalLootSlot& Slot);
TArray<FSurvivalLootSlot> Roll(ESurvivalLootProfile Profile, int32 GameStage,
    int32 MinimumValue, float DifficultyMultiplier, int32 Capacity, FRandomStream& Random, int32 FixedValue);
}
