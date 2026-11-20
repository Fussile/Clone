#pragma once

#include "GameFramework/Character.h"
#include "Misc/Optional.h"
#include "SurvivalWallPlacement.h"
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
    bool IsBuilding() const { return bBuildMode; }
    TOptional<FSurvivalWallPlacement> FindWallPlacement() const;

protected:
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
    void MoveForward(float Value);
    void MoveRight(float Value);
    void Scavenge();
    void ToggleBuildMode();
    void RotateWall();
    void PlaceWall();

    int32 SupplyCount = 0;
    bool bBuildMode = false;
    bool bWallRotated = false;

    UPROPERTY(VisibleAnywhere, Category = "Camera")
    TObjectPtr<UCameraComponent> FirstPersonCamera;
};
