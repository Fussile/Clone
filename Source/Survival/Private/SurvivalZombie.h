#pragma once

#include "GameFramework/Character.h"
#include "SurvivalZombie.generated.h"

class ASurvivalCharacter;
class UAnimSequence;

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

    void UpdateLocomotion(bool bMoving);
    void ApplyAttack(ASurvivalCharacter& Target);

    UPROPERTY(EditAnywhere, Category = "Survival", meta = (ClampMin = "0"))
    float AggroRange = 0.0f;

    bool bAlerted = false;
    int32 Health = 90;
    double NextAttackTime = 0.0;
    double AttackImpactTime = -1.0;
    FVector AttackDirection = FVector::ZeroVector;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> IdleAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> WalkAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> AttackAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> ActiveAnimation;
};
