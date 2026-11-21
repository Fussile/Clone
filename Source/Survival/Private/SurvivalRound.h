#pragma once

#include "GameFramework/Actor.h"
#include "SurvivalRound.generated.h"

class USphereComponent;
class UProjectileMovementComponent;

UCLASS()
class ASurvivalRound : public AActor
{
    GENERATED_BODY()

public:
    ASurvivalRound();

protected:
    virtual void BeginPlay() override;

private:
    UFUNCTION()
    void HandleImpact(const FHitResult& Hit);

    UPROPERTY()
    TObjectPtr<USphereComponent> Collision;

    UPROPERTY()
    TObjectPtr<UProjectileMovementComponent> Movement;
};
