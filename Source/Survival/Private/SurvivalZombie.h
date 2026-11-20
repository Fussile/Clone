#pragma once

#include "GameFramework/Character.h"
#include "SurvivalZombie.generated.h"

class ASurvivalCharacter;

UCLASS()
class ASurvivalZombie : public ACharacter
{
    GENERATED_BODY()

public:
    ASurvivalZombie();
    void HitBySidearm();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    bool ProbePath(const FVector& Destination, FHitResult& Hit) const;
    FVector FindDestination(const ASurvivalCharacter& Target) const;

    int32 Health = 90;
    double NextAttackTime = 0.0;
};
