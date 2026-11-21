#include "SurvivalGameMode.h"

#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "SurvivalBuildChunk.h"
#include "SurvivalCharacter.h"
#include "SurvivalHUD.h"

namespace
{
const FName GroundTag(TEXT("StructuralGround"));

FIntVector StructureCell(const FVector& Base)
{
    constexpr double CellSize = FSurvivalBuildPlacement::GridSize;
    return FIntVector(FMath::FloorToInt(Base.X / CellSize), FMath::FloorToInt(Base.Y / CellSize),
        FMath::FloorToInt(Base.Z / CellSize));
}
}

ASurvivalGameMode::ASurvivalGameMode()
{
    DefaultPawnClass = ASurvivalCharacter::StaticClass();
    HUDClass = ASurvivalHUD::StaticClass();
}

ASurvivalBuildChunk* ASurvivalGameMode::FindOrCreateChunk(const FVector& Base)
{
    constexpr double ChunkSize = 16.0 * FSurvivalBuildPlacement::GridSize;
    const FIntPoint Key(FMath::FloorToInt(Base.X / ChunkSize), FMath::FloorToInt(Base.Y / ChunkSize));
    ASurvivalBuildChunk* Chunk = BuildChunks.FindRef(Key).Get();
    if (!IsValid(Chunk))
    {
        const FVector Origin(Key.X * ChunkSize, Key.Y * ChunkSize, 0.0);
        Chunk = GetWorld()->SpawnActor<ASurvivalBuildChunk>(Origin, FRotator::ZeroRotator);
        if (Chunk)
        {
            BuildChunks.Add(Key, Chunk);
        }
    }
    return Chunk;
}

bool ASurvivalGameMode::AddPlan(const FSurvivalBuildPlacement& Placement)
{
    ASurvivalBuildChunk* Chunk = FindOrCreateChunk(Placement.Base);
    if (!Chunk || !Chunk->AddPlan(Placement))
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

bool ASurvivalGameMode::HasGroundFooting(const FSurvivalBuildPlacement& Placement) const
{
    const FVector Probe(0, 0, 8);
    const FCollisionQueryParams Query(SCENE_QUERY_STAT(StructuralGround), false);
    for (const FBox& Box : Placement.GetPiece().Boxes)
    {
        if (Box.Min.Z != 0.0)
        {
            continue;
        }
        for (double X : {Box.Min.X + 2.0, Box.Max.X - 2.0})
        {
            for (double Y : {Box.Min.Y + 2.0, Box.Max.Y - 2.0})
            {
                const FVector Foot = Placement.Base + Placement.Rotation.RotateVector(FVector(X, Y, 0));
                FHitResult Hit;
                if (!GetWorld()->LineTraceSingleByChannel(Hit, Foot + Probe, Foot - Probe, ECC_Visibility, Query)
                    || Hit.ImpactNormal.Z < 0.95 || !Hit.GetActor() || !Hit.GetActor()->ActorHasTag(GroundTag))
                {
                    return false;
                }
            }
        }
    }
    return true;
}

void ASurvivalGameMode::FindContacts(const FSurvivalBuildPlacement& Placement,
    TArray<FSurvivalSupportLink>& Supports, TArray<FSurvivalSupportLink>& Dependents) const
{
    const FIntVector Cell = StructureCell(Placement.Base);
    // Current pieces are one cell wide and at most 2.2 m tall.
    for (int32 X = -1; X <= 1; ++X)
    {
        for (int32 Y = -1; Y <= 1; ++Y)
        {
            for (int32 Z = -3; Z <= 3; ++Z)
            {
                const TArray<int32>* Candidates = PiecesByCell.Find(Cell + FIntVector(X, Y, Z));
                if (!Candidates)
                {
                    continue;
                }
                for (int32 Id : *Candidates)
                {
                    const FSurvivalBuildPlacement& Other = RegisteredPieces.FindChecked(Id).Placement;
                    const int32 Support = Placement.SupportCostFrom(Other);
                    const int32 Dependent = Other.SupportCostFrom(Placement);
                    if (Support >= 0)
                    {
                        Supports.Add({Id, Support});
                    }
                    if (Dependent >= 0)
                    {
                        Dependents.Add({Id, Dependent});
                    }
                }
            }
        }
    }
}

bool ASurvivalGameMode::HasSupportedContact(const TArray<FSurvivalSupportLink>& Supports) const
{
    return Supports.ContainsByPredicate([this](const FSurvivalSupportLink& Link)
    {
        return Structure.GetBudget(Link.Id) >= Link.Cost;
    });
}

bool ASurvivalGameMode::CanSupport(const FSurvivalBuildPlacement& Placement) const
{
    if (HasGroundFooting(Placement))
    {
        return true;
    }
    TArray<FSurvivalSupportLink> Supports, Dependents;
    FindContacts(Placement, Supports, Dependents);
    return HasSupportedContact(Supports);
}

void ASurvivalGameMode::RegisterPiece(int32 Id, ASurvivalBuildChunk& Chunk,
    const FSurvivalBuildPlacement& Placement, bool bGrounded, const TArray<FSurvivalSupportLink>& Supports,
    const TArray<FSurvivalSupportLink>& Dependents)
{
    RegisteredPieces.Add(Id, FRegisteredPiece{&Chunk, Placement});
    PiecesByCell.FindOrAdd(StructureCell(Placement.Base)).Add(Id);
    Structure.Add(Id, Placement.GetPiece().SupportCapacity(), bGrounded, Supports, Dependents);
}

bool ASurvivalGameMode::FinishPlan(const FSurvivalPlanTarget& Target, FSurvivalMaterials& Inventory)
{
    const FSurvivalBuildPlacement* Plan = Target.GetPlan();
    if (!Plan)
    {
        return false;
    }
    const FSurvivalBuildPlacement Placement = *Plan;
    const FSurvivalMaterials Cost = Placement.GetPiece().Cost;
    if (!Cost.MissingFrom(Inventory).IsEmpty())
    {
        return false;
    }
    const bool bGrounded = HasGroundFooting(Placement);
    TArray<FSurvivalSupportLink> Supports, Dependents;
    FindContacts(Placement, Supports, Dependents);
    const int32 Id = NextStructureId;
    if ((!bGrounded && !HasSupportedContact(Supports)) || !Target.Chunk->FinishPlan(Target.PlanIndex, Id))
    {
        return false;
    }

    ++NextStructureId;
    RegisterPiece(Id, *Target.Chunk, Placement, bGrounded, Supports, Dependents);
    Inventory.Spend(Cost);
    PlannedCost.Spend(Cost);
    return true;
}

bool ASurvivalGameMode::AddAuthoredPiece(const FSurvivalBuildPlacement& Placement)
{
    const bool bGrounded = HasGroundFooting(Placement);
    TArray<FSurvivalSupportLink> Supports, Dependents;
    FindContacts(Placement, Supports, Dependents);
    if (!bGrounded && !HasSupportedContact(Supports))
    {
        return false;
    }
    const int32 Id = NextStructureId;
    ASurvivalBuildChunk* Chunk = FindOrCreateChunk(Placement.Base);
    if (!Chunk || !Chunk->AddFinishedPiece(Placement, Id))
    {
        return false;
    }

    ++NextStructureId;
    RegisterPiece(Id, *Chunk, Placement, bGrounded, Supports, Dependents);
    return true;
}

void ASurvivalGameMode::RemoveStructure(int32 Id)
{
    if (!RegisteredPieces.Contains(Id))
    {
        return;
    }
    TArray<int32> Removed = Structure.Remove(Id);
    Removed.Add(Id);
    TMap<ASurvivalBuildChunk*, TSet<int32>> Batches;
    for (int32 PieceId : Removed)
    {
        FRegisteredPiece Piece;
        if (!RegisteredPieces.RemoveAndCopyValue(PieceId, Piece))
        {
            continue;
        }
        const FIntVector Cell = StructureCell(Piece.Placement.Base);
        TArray<int32>& Entries = PiecesByCell.FindChecked(Cell);
        Entries.RemoveSwap(PieceId);
        if (Entries.IsEmpty())
        {
            PiecesByCell.Remove(Cell);
        }
        if (ASurvivalBuildChunk* Chunk = Piece.Chunk.Get())
        {
            Batches.FindOrAdd(Chunk).Add(PieceId);
        }
    }

    int32 DebrisSlots = 12;
    for (const TPair<ASurvivalBuildChunk*, TSet<int32>>& Batch : Batches)
    {
        Batch.Key->RemoveFinishedPieces(Batch.Value, DebrisSlots);
    }
}
