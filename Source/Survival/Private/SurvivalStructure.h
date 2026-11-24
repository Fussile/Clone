#pragma once

#include "CoreMinimal.h"

struct FSurvivalSupportLink
{
    int32 Id;
    int32 Cost;
};

class FSurvivalStructure
{
public:
    void Add(int32 Id, int32 Capacity, bool bGrounded, const TArray<FSurvivalSupportLink>& Supports,
        const TArray<FSurvivalSupportLink>& Dependents);
    TArray<int32> Remove(int32 Id);
    int32 GetBudget(int32 Id) const;

private:
    struct FNode
    {
        int32 Capacity;
        int32 Budget = INDEX_NONE;
        bool bGrounded;
        TArray<FSurvivalSupportLink> Supports;
        TSet<int32> Dependents;
    };

    TArray<int32> Update(const TArray<int32>& Initial);
    void Unlink(int32 Id, TArray<int32>& Dirty);
    TMap<int32, FNode> Nodes;
};
