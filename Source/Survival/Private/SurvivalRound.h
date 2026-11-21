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
    void SetRifleRound();

protected:
    virtual void BeginPlay() override;

private:
    bool bRifleRound = false;

    UFUNCTION()
    void HandleImpact(const FHitResult& Hit);

    UPROPERTY()
    TObjectPtr<USphereComponent> Collision;

    UPROPERTY()
    TObjectPtr<UProjectileMovementComponent> Movement;
};
