#pragma once

#include "GameFramework/Actor.h"
#include "SurvivalSidearm.generated.h"

class UStaticMeshComponent;

UCLASS()
class ASurvivalSidearm : public AActor
{
    GENERATED_BODY()

public:
    ASurvivalSidearm();

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> ImportedVisual;
};
