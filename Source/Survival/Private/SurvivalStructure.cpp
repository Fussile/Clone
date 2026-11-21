#include "SurvivalStructure.h"

int32 FSurvivalStructure::GetBudget(int32 Id) const
{
    const FNode* Node = Nodes.Find(Id);
    return Node ? Node->Budget : INDEX_NONE;
}

void FSurvivalStructure::Add(int32 Id, int32 Capacity, bool bGrounded,
    const TArray<FSurvivalSupportLink>& Supports, const TArray<FSurvivalSupportLink>& Dependents)
{
    FNode& Node = Nodes.Add(Id, FNode{Capacity, INDEX_NONE, bGrounded, Supports, {}});
    for (const FSurvivalSupportLink& Link : Supports)
    {
        Nodes.FindChecked(Link.Id).Dependents.Add(Id);
    }
    for (const FSurvivalSupportLink& Link : Dependents)
    {
        Nodes.FindChecked(Link.Id).Supports.Add({Id, Link.Cost});
        Node.Dependents.Add(Link.Id);
    }
    Update({Id});
}

void FSurvivalStructure::Unlink(int32 Id, TArray<int32>& Dirty)
{
    FNode Removed;
    if (!Nodes.RemoveAndCopyValue(Id, Removed))
    {
        return;
    }
    for (const FSurvivalSupportLink& Link : Removed.Supports)
    {
        if (FNode* Parent = Nodes.Find(Link.Id))
        {
            Parent->Dependents.Remove(Id);
        }
    }
    for (int32 ChildId : Removed.Dependents)
    {
        if (FNode* Child = Nodes.Find(ChildId))
        {
            Child->Supports.RemoveAll([Id](const FSurvivalSupportLink& Link) { return Link.Id == Id; });
            Dirty.Add(ChildId);
        }
    }
}

TArray<int32> FSurvivalStructure::Remove(int32 Id)
{
    TArray<int32> Dirty;
    Unlink(Id, Dirty);
    TArray<int32> Unsupported = Update(Dirty);
    for (int32 UnsupportedId : Unsupported)
    {
        Unlink(UnsupportedId, Dirty);
    }
    return Unsupported;
}

TArray<int32> FSurvivalStructure::Update(const TArray<int32>& Initial)
{
    TArray<int32> Pending;
    TSet<int32> Queued;
    TSet<int32> Unstable;
    const auto Enqueue = [&](int32 Id)
    {
        if (!Queued.Contains(Id))
        {
            Queued.Add(Id);
            Pending.Add(Id);
        }
    };
    for (int32 Id : Initial)
    {
        Enqueue(Id);
    }
    while (!Pending.IsEmpty())
    {
        const int32 Id = Pending.Pop(EAllowShrinking::No);
        Queued.Remove(Id);
        FNode& Node = Nodes.FindChecked(Id);
        int32 Budget = Node.bGrounded ? Node.Capacity : INDEX_NONE;
        if (!Node.bGrounded)
        {
            for (const FSurvivalSupportLink& Parent : Node.Supports)
            {
                Budget = FMath::Max(Budget, FMath::Min(Node.Capacity, GetBudget(Parent.Id) - Parent.Cost));
            }
        }
        if (Budget < 0)
        {
            Unstable.Add(Id);
        }
        else
        {
            Unstable.Remove(Id);
        }
        if (Budget != Node.Budget)
        {
            Node.Budget = Budget;
            for (int32 Child : Node.Dependents)
            {
                Enqueue(Child);
            }
        }
    }
    TArray<int32> Result = Unstable.Array();
    Result.Sort();
    return Result;
}
