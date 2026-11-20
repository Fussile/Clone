#include "SurvivalGameMode.h"

#include "Engine/World.h"
#include "SurvivalBuildChunk.h"
#include "SurvivalCharacter.h"
#include "SurvivalHUD.h"

ASurvivalGameMode::ASurvivalGameMode()
{
    DefaultPawnClass = ASurvivalCharacter::StaticClass();
    HUDClass = ASurvivalHUD::StaticClass();
}

bool ASurvivalGameMode::AddPlan(const FSurvivalBuildPlacement& Placement)
{
    constexpr double ChunkSize = 16.0 * FSurvivalBuildPlacement::GridSize;
    const FIntPoint Key(FMath::FloorToInt(Placement.Base.X / ChunkSize),
        FMath::FloorToInt(Placement.Base.Y / ChunkSize));
    ASurvivalBuildChunk* Chunk = BuildChunks.FindRef(Key).Get();
    if (!IsValid(Chunk))
    {
        const FVector Origin(Key.X * ChunkSize, Key.Y * ChunkSize, 0.0);
        Chunk = GetWorld()->SpawnActor<ASurvivalBuildChunk>(Origin, FRotator::ZeroRotator);
        if (!Chunk)
        {
            return false;
        }
        BuildChunks.Add(Key, Chunk);
    }
    if (!Chunk->AddPlan(Placement))
    {
        return false;
    }

    PlannedCost.Add(Placement.GetPiece().Cost);
    return true;
}

void ASurvivalGameMode::RemovePlan(const FSurvivalPlanTarget& Target)
{
    if (const FSurvivalBuildPlacement* Plan = Target.GetPlan())
    {
        PlannedCost.Spend(Plan->GetPiece().Cost);
        Target.Chunk->RemovePlan(Target.PlanIndex);
    }
}

bool ASurvivalGameMode::FinishPlan(const FSurvivalPlanTarget& Target, FSurvivalMaterials& Inventory)
{
    const FSurvivalBuildPlacement* Plan = Target.GetPlan();
    if (!Plan)
    {
        return false;
    }
    const FSurvivalMaterials Cost = Plan->GetPiece().Cost;
    if (!Cost.MissingFrom(Inventory).IsEmpty() || !Target.Chunk->FinishPlan(Target.PlanIndex))
    {
        return false;
    }

    Inventory.Spend(Cost);
    PlannedCost.Spend(Cost);
    return true;
}
