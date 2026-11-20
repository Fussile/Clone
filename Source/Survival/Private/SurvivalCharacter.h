#pragma once

#include "GameFramework/Character.h"
#include "Misc/Optional.h"
#include "SurvivalBuilding.h"
#include "SurvivalCharacter.generated.h"

class ASurvivalSupplyCrate;
class UAnimSequence;
class UCameraComponent;
class USpringArmComponent;

UCLASS()
class ASurvivalCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ASurvivalCharacter();

    ASurvivalSupplyCrate* FindSupplyCrate() const;
    FSurvivalPlanTarget FindPlan() const;
    FSurvivalMaterials GetPlannedCost() const;
    const FSurvivalMaterials& GetInventory() const { return Inventory; }
    bool IsBuilding() const { return bBuildMode; }
    int32 GetSelectedPiece() const { return SelectedPiece; }
    TOptional<FSurvivalBuildPlacement> FindPlacement() const;
    bool CanFinish(const FSurvivalBuildPlacement& Placement) const;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
    const UCameraComponent& GetAimCamera() const;
    bool FindWorldAimHit(double Reach, FHitResult& Hit) const;
    bool HasReachTo(const FVector& Point, double Reach) const;
    bool HasSupport(const FSurvivalBuildPlacement& Placement) const;
    bool IsFootprintClear(const FSurvivalBuildPlacement& Placement, bool bPlanning) const;

    void MoveForward(float Value);
    void MoveRight(float Value);
    void Interact();
    void ToggleBuildMode();
    void RotatePiece();
    void NextPiece();
    void PreviousPiece();
    void PlacePlan();
    void RemovePlan();
    void ToggleView();
    void UpdateSurvivorAnimation();

    FSurvivalMaterials Inventory;
    int32 SelectedPiece = 0;
    int32 QuarterTurns = 0;
    bool bBuildMode = false;
    bool bThirdPerson = false;

    UPROPERTY(VisibleAnywhere, Category = "Camera")
    TObjectPtr<UCameraComponent> FirstPersonCamera;

    UPROPERTY(VisibleAnywhere, Category = "Camera")
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, Category = "Camera")
    TObjectPtr<UCameraComponent> ThirdPersonCamera;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> IdleAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> RunAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> JumpAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> CurrentAnimation;
};
