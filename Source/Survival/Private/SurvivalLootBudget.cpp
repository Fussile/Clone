#include "SurvivalLootBudget.h"

#include "SurvivalGear.h"

namespace
{
using EItem = ESurvivalItem;

struct FItemValue
{
    EItem Item;
    int32 BaseValue;
    int32 QualitySteps;
};

constexpr FItemValue Values[] = {
    {EItem::Sidearm, 100, 3}, {EItem::Food, 8, 0}, {EItem::Water, 6, 0},
    {EItem::Bandage, 14, 0}, {EItem::Timber, 2, 0}, {EItem::Stone, 2, 0},
    {EItem::Cloth, 1, 0}, {EItem::Rounds, 1, 0}, {EItem::TimberDoor, 9, 0},
    {EItem::Rifle, 165, 3}, {EItem::RifleRounds, 2, 0}, {EItem::BuilderGuide, 23, 0},
    {EItem::ScavengerNotes, 25, 0}, {EItem::DressingSchematic, 28, 0}, {EItem::FieldDressing, 24, 0},
    {EItem::RifleStock, 28, 12}, {EItem::RifleReceiver, 50, 12}, {EItem::RifleBarrel, 36, 12},
    {EItem::RifleOptic, 35, 12}, {EItem::SidearmGrip, 18, 12}, {EItem::SidearmFrame, 33, 12},
    {EItem::SidearmBarrel, 27, 12}, {EItem::SidearmSlide, 24, 12},
};
static_assert(UE_ARRAY_COUNT(Values) == static_cast<int32>(EItem::None));
static_assert([]{
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Values); ++Index)
        if (static_cast<int32>(Values[Index].Item) != Index || Values[Index].BaseValue <= 0) return false;
    return true;
}(), "Every loot item needs a positive value in enum order.");

struct FPoolEntry
{
    EItem Item;
    int32 Weight;
    int32 MaxCount = 1;
};

constexpr FPoolEntry General[] = {
    {EItem::Food, 24, 3}, {EItem::Water, 23, 3}, {EItem::Cloth, 16, 10},
    {EItem::Timber, 18, 8}, {EItem::Stone, 14, 7}, {EItem::Bandage, 8, 2},
    {EItem::None, 8}, {EItem::ScavengerNotes, 3}
};
constexpr FPoolEntry Food[] = {
    {EItem::Food, 45, 5}, {EItem::Water, 40, 5}, {EItem::Cloth, 8, 7}
};
constexpr FPoolEntry Building[] = {
    {EItem::Timber, 36, 40}, {EItem::Stone, 32, 32}, {EItem::Cloth, 12, 15},
    {EItem::TimberDoor, 9, 3}, {EItem::BuilderGuide, 4}
};
constexpr FPoolEntry Medical[] = {
    {EItem::Bandage, 36, 4}, {EItem::FieldDressing, 16, 3}, {EItem::Water, 14, 4},
    {EItem::Cloth, 12, 12}, {EItem::DressingSchematic, 4}
};
constexpr FPoolEntry Office[] = {
    {EItem::ScavengerNotes, 22}, {EItem::BuilderGuide, 20}, {EItem::Food, 14, 3},
    {EItem::Water, 16, 4}, {EItem::Cloth, 15, 10}, {EItem::None, 8}
};
constexpr FPoolEntry Vehicle[] = {
    {EItem::Water, 26, 3}, {EItem::Food, 26, 3}, {EItem::Cloth, 20, 10},
    {EItem::Bandage, 11, 3}, {EItem::None, 13}
};
constexpr FPoolEntry Mailbox[] = {
    {EItem::ScavengerNotes, 22}, {EItem::BuilderGuide, 21}, {EItem::DressingSchematic, 13},
    {EItem::Cloth, 12, 15}
};
constexpr FPoolEntry Security[] = {
    {EItem::Sidearm, 19}, {EItem::Rounds, 27, 28}, {EItem::RifleRounds, 9, 14},
    {EItem::SidearmFrame, 11}, {EItem::SidearmBarrel, 10}, {EItem::SidearmSlide, 9},
    {EItem::SidearmGrip, 9}, {EItem::Bandage, 21, 4}, {EItem::Cloth, 9, 12}, {EItem::None, 8}
};
constexpr FPoolEntry Police[] = {
    {EItem::Sidearm, 115}, {EItem::Rounds, 23, 38}, {EItem::RifleRounds, 5, 12},
    {EItem::SidearmFrame, 10}, {EItem::SidearmBarrel, 9}, {EItem::SidearmSlide, 8},
    {EItem::SidearmGrip, 8}, {EItem::Bandage, 12, 4}, {EItem::Cloth, 8, 15}, {EItem::None, 8}
};
constexpr FPoolEntry Armory[] = {
    {EItem::Rifle, 85}, {EItem::Sidearm, 62}, {EItem::Rounds, 20, 50}, {EItem::RifleRounds, 25, 55},
    {EItem::RifleReceiver, 13}, {EItem::RifleBarrel, 12}, {EItem::RifleStock, 10}, {EItem::RifleOptic, 10},
    {EItem::Bandage, 9, 5}, {EItem::FieldDressing, 8, 3}, {EItem::Cloth, 6, 20}, {EItem::None, 14}
};

struct FProfile
{
    TConstArrayView<FPoolEntry> Entries;
    int32 BaseValue;
    int32 MinimumValue;
    float Difficulty;
    float StageRate;
    EItem GuaranteedItem = EItem::None;
};

FProfile GetProfile(ESurvivalLootProfile Profile)
{
    switch (Profile)
    {
    case ESurvivalLootProfile::Food: return {Food, 34, 0, 1.0f, 1.6f};
    case ESurvivalLootProfile::Building: return {Building, 42, 0, 1.0f, 2.0f};
    case ESurvivalLootProfile::Medical: return {Medical, 42, 0, 1.0f, 1.8f};
    case ESurvivalLootProfile::Office: return {Office, 32, 0, 1.0f, 1.4f};
    case ESurvivalLootProfile::Vehicle: return {Vehicle, 36, 0, 1.0f, 1.6f};
    case ESurvivalLootProfile::Mailbox: return {Mailbox, 18, 0, .85f, .22f};
    case ESurvivalLootProfile::Security: return {Security, 92, 0, 1.0f, 2.4f};
    case ESurvivalLootProfile::PoliceArmory: return {Police, 200, 120, 1.25f, 2.8f};
    case ESurvivalLootProfile::MilitaryArmory: return {Armory, 60, 120, 1.8f, 3.0f, EItem::Sidearm};
    default: return {General, 34, 0, 1.0f, 1.7f};
    }
}

struct FChoice
{
    FSurvivalLootSlot Slot;
    int32 Value;
    int32 Weight;
    int32 DuplicateGroup;
};

constexpr int32 BudgetLimit = 8192;
constexpr int32 BulkStackBudget = 1000;
constexpr int32 DuplicateLimit = 2;
constexpr int32 DuplicateGroupCount = UE_ARRAY_COUNT(Values) + UE_ARRAY_COUNT(SurvivalGear);

int32 GetDuplicateGroup(const FSurvivalLootSlot& Slot)
{
    if (Slot.GearIndex >= 0 && Slot.GearIndex < UE_ARRAY_COUNT(SurvivalGear))
        return UE_ARRAY_COUNT(Values) + Slot.GearIndex;
    if (Slot.ItemId < UE_ARRAY_COUNT(Values) && SurvivalItemMaxStack(Slot.Item()) == 1)
        return Slot.ItemId;
    return INDEX_NONE;
}

FSurvivalLootSlot MakeStack(EItem Item, int32 Count, int32 Quality, int32 Gear = INDEX_NONE)
{
    FSurvivalLootSlot Stack;
    Stack.ItemId = static_cast<uint8>(Item);
    Stack.Count = Count;
    Stack.Quality = Quality;
    Stack.GearIndex = Gear;
    return Stack;
}

void AddChoice(TArray<FChoice>& Choices, FSurvivalLootSlot Stack, int32 Weight, int32 Budget)
{
    const int32 Value = SurvivalLootBudget::GetUnitValue(Stack) * Stack.Count;
    if (Value > 0 && Value <= Budget) Choices.Add({Stack, Value, Weight, GetDuplicateGroup(Stack)});
}
}

int32 SurvivalLootBudget::GetGameStage(int32 Level, int32 DaysSurvived)
{
    Level = FMath::Clamp(Level, 1, FSurvivalProgression::LevelCap);
    return Level + FMath::Clamp(DaysSurvived, 0, Level);
}

int32 SurvivalLootBudget::GetUnitValue(const FSurvivalLootSlot& Slot)
{
    if (Slot.GearIndex >= 0 && Slot.GearIndex < UE_ARRAY_COUNT(SurvivalGear))
        return SurvivalGear[Slot.GearIndex].LootValue;
    if (Slot.ItemId >= UE_ARRAY_COUNT(Values)) return 0;

    const auto& Value = Values[Slot.ItemId];
    const int32 Quality = FMath::Clamp(Slot.Quality, 1, FSurvivalWeaponAssembly::MaxQuality);
    return Value.BaseValue + (Value.QualitySteps > 0 ? (Quality - 1) / Value.QualitySteps : 0);
}

TArray<FSurvivalLootSlot> SurvivalLootBudget::Roll(ESurvivalLootProfile Profile, int32 GameStage,
    int32 MinimumValue, float DifficultyMultiplier, int32 Capacity, FRandomStream& Random,
    TConstArrayView<FSurvivalLootSlot> AuthoredSlots)
{
    TArray<FSurvivalLootSlot> Result;
    if (Capacity <= 0 || Profile == ESurvivalLootProfile::Authored || Profile == ESurvivalLootProfile::Storage ||
        Profile == ESurvivalLootProfile::Dropped) return Result;

    const FProfile Pool = GetProfile(Profile);
    const float Difficulty = Pool.Difficulty * FMath::Clamp(DifficultyMultiplier, .25f, 4.0f);
    const int32 Stage = FMath::Clamp(GameStage, 1, FSurvivalProgression::LevelCap * 2);
    int32 Budget = FMath::Clamp(FMath::Max3(MinimumValue, Pool.MinimumValue,
        FMath::RoundToInt((Pool.BaseValue + (Stage - 1) * Pool.StageRate) * Difficulty)), 1, BudgetLimit);
    int32 Copies[DuplicateGroupCount] = {};
    for (const auto& Slot : AuthoredSlots)
    {
        if (Slot.IsEmpty()) continue;
        Budget = FMath::Max(0, Budget - GetUnitValue(Slot) * Slot.Count);
        const int32 Group = GetDuplicateGroup(Slot);
        if (Group != INDEX_NONE) Copies[Group] += Slot.Count;
    }
    if (Budget == 0) return Result;
    const int32 QualityCeiling = FMath::Clamp(1 + FMath::RoundToInt((Stage - 1) * 4.0f * Difficulty
        + FMath::Max(0.0f, Difficulty - 1.0f) * 125.0f), 1, FSurvivalWeaponAssembly::MaxQuality);

    if (Pool.GuaranteedItem != EItem::None && Copies[static_cast<int32>(Pool.GuaranteedItem)] == 0
        && Budget >= Values[static_cast<int32>(Pool.GuaranteedItem)].BaseValue)
    {
        auto Stack = MakeStack(Pool.GuaranteedItem, 1, 1);
        const auto& Price = Values[Stack.ItemId];
        const int32 AffordableQuality = 1 + FMath::Max(0, Budget - Price.BaseValue - 12) * Price.QualitySteps;
        const int32 Top = FMath::Min(QualityCeiling, AffordableQuality);
        Stack.Quality = Random.RandRange(FMath::Max(1, Top / 3), Top);
        Result.Add(Stack);
        ++Copies[Stack.ItemId];
        Budget -= GetUnitValue(Stack);
    }

    TArray<FChoice> Choices;
    for (const auto& Entry : Pool.Entries)
    {
        if (Entry.Item == EItem::None)
        {
            // Sample gear options so a long clothing catalog does not dilute the other weights.
            for (int32 Option = 0; Option < 3; ++Option)
                AddChoice(Choices, MakeStack(EItem::None, 1, 0, Random.RandRange(0, UE_ARRAY_COUNT(SurvivalGear) - 1)),
                    FMath::Max(1, Entry.Weight / 3), Budget);
        }
        else if (Values[static_cast<int32>(Entry.Item)].QualitySteps > 0)
        {
            AddChoice(Choices, MakeStack(Entry.Item, 1, 1), FMath::Max(1, Entry.Weight / 4), Budget);
            if (QualityCeiling > 1)
            {
                const int32 Mid = FMath::Max(2, QualityCeiling / 2);
                AddChoice(Choices, MakeStack(Entry.Item, 1, Random.RandRange(2, Mid)), Entry.Weight, Budget);
                AddChoice(Choices, MakeStack(Entry.Item, 1, Random.RandRange(Mid, QualityCeiling)), Entry.Weight, Budget);
            }
        }
        else
        {
            const int32 MaxCount = SurvivalItemMaxStack(Entry.Item);
            const int32 Count = FMath::Min(Entry.MaxCount + (Stage - 1) / 4, MaxCount);
            // A singleton fills small gaps. Two larger stack sizes help higher-stage rolls fit the grid.
            AddChoice(Choices, MakeStack(Entry.Item, 1, 0), Count > 1 ? FMath::Max(1, Entry.Weight / 4) : Entry.Weight, Budget);
            if (Count > 1)
                AddChoice(Choices, MakeStack(Entry.Item, 1 + Count / 2, 0), Entry.Weight, Budget);
            if (Count > 2)
                AddChoice(Choices, MakeStack(Entry.Item, Count, 0), Entry.Weight, Budget);
            if (Budget >= BulkStackBudget && Count < MaxCount)
                AddChoice(Choices, MakeStack(Entry.Item, MaxCount, 0), FMath::Max(1, Entry.Weight / 4), Budget);
        }
    }

    TArray<int32> SlotsNeeded;
    const auto RecalculateValues = [&]
    {
        // A group reaching its limit retires every quality of that item at once.
        Choices.RemoveAll([&](const FChoice& Choice)
        {
            return Choice.DuplicateGroup != INDEX_NONE && Copies[Choice.DuplicateGroup] >= DuplicateLimit;
        });
        const int32 RemainingSlots = Capacity - Result.Num();
        SlotsNeeded.Init(RemainingSlots + 1, Budget + 1);
        SlotsNeeded[0] = 0;
        for (int32 Value = 1; Value <= Budget; ++Value)
            for (const auto& Choice : Choices)
                if (Choice.Value <= Value)
                    SlotsNeeded[Value] = FMath::Min(SlotsNeeded[Value], 1 + SlotsNeeded[Value - Choice.Value]);
        while (Budget > 0 && SlotsNeeded[Budget] > RemainingSlots) --Budget;
    };
    RecalculateValues();

    TArray<int32, TInlineAllocator<64>> Weights;
    Weights.Reserve(Choices.Num());
    while (Budget > 0 && Result.Num() < Capacity)
    {
        const int32 RemainingSlots = Capacity - Result.Num() - 1;
        Weights.Reset();
        int32 Weight = 0;
        for (const auto& Choice : Choices)
        {
            int32 Adjusted = 0;
            if (Choice.Value <= Budget && SlotsNeeded[Budget - Choice.Value] <= RemainingSlots)
            {
                const bool bDuplicate = Choice.DuplicateGroup != INDEX_NONE && Copies[Choice.DuplicateGroup] > 0;
                Adjusted = bDuplicate ? Choice.Weight : 8 * Choice.Weight;
            }
            Weights.Add(Adjusted);
            Weight += Adjusted;
        }
        if (Weight == 0) break;

        int32 Pick = Random.RandRange(1, Weight);
        int32 Group = INDEX_NONE;
        for (int32 Index = 0; Index < Choices.Num(); ++Index)
        {
            Pick -= Weights[Index];
            if (Pick > 0) continue;
            const FChoice& Choice = Choices[Index];
            Result.Add(Choice.Slot);
            Budget -= Choice.Value;
            Group = Choice.DuplicateGroup;
            break;
        }
        if (Group != INDEX_NONE && ++Copies[Group] == DuplicateLimit && Budget > 0 && Result.Num() < Capacity)
            RecalculateValues();
    }
    return Result;
}
