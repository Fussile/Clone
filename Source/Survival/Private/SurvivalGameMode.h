#pragma once

#include "GameFramework/GameModeBase.h"
#include "SurvivalGameMode.generated.h"

class ASurvivalWallChunk;
struct FSurvivalWallPlacement;

UCLASS()
class ASurvivalGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ASurvivalGameMode();
    bool AddWall(const FSurvivalWallPlacement& Placement);

private:
    UPROPERTY(Transient)
    TMap<FIntPoint, TObjectPtr<ASurvivalWallChunk>> WallChunks;
};
