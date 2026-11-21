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
    void HitByShot(ASurvivalCharacter* Shooter, int32 Damage);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    bool ProbePath(const FVector& Destination, FHitResult& Hit) const;
    void UpdatePerception(double Now);
    FVector ChooseMoveDirection(const FVector& Desired);
    void RememberTarget(const ASurvivalCharacter& Target, double Now);

    void UpdateLocomotion(bool bMoving);
    void ApplyAttack();

    UPROPERTY(EditAnywhere, Category = "Survival", meta = (ClampMin = "0"))
    float AggroRange = 0.0f;

    UPROPERTY(EditAnywhere, Category = "Survival")
    bool bIsRunner = false;

    FVector Home = FVector::ZeroVector;
    FVector LastKnownLocation = FVector::ZeroVector;
    double SearchUntil = -1.0;
    double NextSenseTime = 0.0;
    double DetourSide = 1.0;
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
