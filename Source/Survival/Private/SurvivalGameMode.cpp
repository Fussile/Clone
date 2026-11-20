#include "SurvivalGameMode.h"
#include "SurvivalCharacter.h"
#include "SurvivalHUD.h"
#include "SurvivalWallChunk.h"
#include "SurvivalWallPlacement.h"
#include "Engine/World.h"

ASurvivalGameMode::ASurvivalGameMode()
{
    DefaultPawnClass = ASurvivalCharacter::StaticClass();
    HUDClass = ASurvivalHUD::StaticClass();
}

bool ASurvivalGameMode::AddWall(const FSurvivalWallPlacement& Placement)
{
    constexpr double ChunkSize = 16.0 * FSurvivalWallPlacement::GridSize;
    const FIntPoint Key(FMath::FloorToInt(Placement.Center.X / ChunkSize),
        FMath::FloorToInt(Placement.Center.Y / ChunkSize));
    if (ASurvivalWallChunk* Existing = WallChunks.FindRef(Key).Get(); IsValid(Existing))
    {
        return Existing->AddWall(Placement);
    }

    const FVector Origin(Key.X * ChunkSize, Key.Y * ChunkSize, 0.0);
    ASurvivalWallChunk* Chunk = GetWorld()->SpawnActor<ASurvivalWallChunk>(Origin, FRotator::ZeroRotator);
    if (!Chunk)
    {
        return false;
    }
    if (!Chunk->AddWall(Placement))
    {
        Chunk->Destroy();
        return false;
    }

    WallChunks.Add(Key, Chunk);
    return true;
}
