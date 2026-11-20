#pragma once

#include "GameFramework/Character.h"
#include "SurvivalCharacter.generated.h"

class ASurvivalSupplyCrate;
class UCameraComponent;
class UInputComponent;

UCLASS()
class ASurvivalCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ASurvivalCharacter();
    ASurvivalSupplyCrate* FindSupplyCrate() const;
    int32 GetSupplyCount() const { return SupplyCount; }

protected:
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
    void MoveForward(float Value);
    void MoveRight(float Value);
    void Scavenge();

    int32 SupplyCount = 0;

    UPROPERTY(VisibleAnywhere, Category = "Camera")
    TObjectPtr<UCameraComponent> FirstPersonCamera;
};
