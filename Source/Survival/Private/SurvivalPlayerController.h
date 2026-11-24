#pragma once

#include "GameFramework/PlayerController.h"
#include "SurvivalPlayerController.generated.h"

UCLASS()
class ASurvivalPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    UFUNCTION(Exec)
    void SetTimeOfDay(float Hour);
};
