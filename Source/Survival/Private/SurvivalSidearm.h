#pragma once

#include "GameFramework/Actor.h"
#include "SurvivalSidearm.generated.h"

class UStaticMeshComponent;
class USoundBase;

UCLASS()
class ASurvivalSidearm : public AActor
{
    GENERATED_BODY()

public:
    ASurvivalSidearm();
    void PlayShotSound();

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> ImportedVisual;

    UPROPERTY(Transient)
    TObjectPtr<USoundBase> ShotSound;
};
