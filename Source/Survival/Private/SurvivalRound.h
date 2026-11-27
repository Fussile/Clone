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
    void SetArrow(float Damage, float Speed);
    void SetRifleRound(float BaseDamage);

protected:
    virtual void BeginPlay() override;

private:
    float ArrowDamage = 0.f;
    bool bRifleRound = false;
    float RifleBaseDamage = 0.0f;

    UFUNCTION()
    void HandleImpact(const FHitResult& Hit);

    UPROPERTY()
    TObjectPtr<USphereComponent> Collision;

    UPROPERTY()
    TObjectPtr<UProjectileMovementComponent> Movement;
};
