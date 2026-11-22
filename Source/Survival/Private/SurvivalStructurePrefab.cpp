#include "SurvivalStructurePrefab.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "SurvivalGameMode.h"

ASurvivalStructurePrefab::ASurvivalStructurePrefab()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("PrefabOrigin")));
}

void ASurvivalStructurePrefab::BeginPlay()
{
    Super::BeginPlay();
    ASurvivalGameMode* GameMode = GetWorld()->GetAuthGameMode<ASurvivalGameMode>();
    if (!GameMode || !GameMode->BeginAuthoredPrefab(PrefabId))
    {
        return;
    }

    // Author each descriptor from footing to roof so every part has its intended support on spawn.
    for (const FSurvivalPrefabPart& Part : Parts)
    {
        if (!GetSurvivalBuildPieces().IsValidIndex(Part.PieceIndex))
        {
            continue;
        }
        FSurvivalBuildPlacement Placement;
        Placement.Base = GetActorTransform().TransformPositionNoScale(Part.Offset);
        Placement.Rotation = GetActorQuat() * FRotator(0, Part.QuarterTurns * 90.0, 0).Quaternion();
        Placement.PieceIndex = Part.PieceIndex;
        if (!GameMode->AddAuthoredPiece(Placement))
        {
            UE_LOG(LogTemp, Warning, TEXT("%s has an unsupported authored piece at %s."),
                *GetName(), *Placement.Base.ToString());
        }
    }
}
