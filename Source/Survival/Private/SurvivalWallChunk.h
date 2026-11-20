#pragma once

#include "GameFramework/Actor.h"
#include "SurvivalWallChunk.generated.h"

class UInstancedStaticMeshComponent;
struct FSurvivalWallPlacement;

UCLASS()
class ASurvivalWallChunk : public AActor
{
    GENERATED_BODY()

public:
    ASurvivalWallChunk();
    bool AddWall(const FSurvivalWallPlacement& Placement);

private:
    UPROPERTY(VisibleAnywhere, Category = "Building")
    TObjectPtr<UInstancedStaticMeshComponent> Walls;
};
