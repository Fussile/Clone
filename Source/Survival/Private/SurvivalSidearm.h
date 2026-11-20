#pragma once

#include "GameFramework/Actor.h"
#include "SurvivalSidearm.generated.h"

UCLASS()
class ASurvivalSidearm : public AActor
{
    GENERATED_BODY()

public:
    ASurvivalSidearm();

protected:
    virtual void BeginPlay() override;
};
