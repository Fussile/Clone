#pragma once

#include "GameFramework/Character.h"
#include "Misc/Optional.h"
#include "SurvivalBuilding.h"
#include "SurvivalCharacter.generated.h"

class ASurvivalSupplyCrate;
class ASurvivalSidearm;
class ASurvivalZombie;
struct FSurvivalFinishedPiece;
class UAnimSequence;
class UCameraComponent;
class USkeletalMeshComponent;
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
    const FSurvivalFinishedPiece* FindFinishedPiece() const;
    void ReceiveZombieHit(int32 Amount);
    int32 GetHealthPoints() const { return HealthPoints; }
    int32 GetAmmo() const { return Ammo; }
    int32 GetReserveAmmo() const { return ReserveAmmo; }
    bool IsReloading() const { return ReloadCompleteAt >= 0.0; }
    bool HasSidearm() const { return bHasSidearm; }
    int32 GetLivingAttackers() const;
    int32 GetDefenseWave() const { return DefenseWave; }
    static constexpr int32 DefenseWaveCount = 3;
    bool ShowHitMarker() const;
    void NotifySidearmHit();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
    const UCameraComponent& GetAimCamera() const;
    bool FindPlanAimHit(double Reach, FHitResult& Hit) const;
    bool FindWorldAimHit(double Reach, FHitResult& Hit) const;
    bool HasReachTo(const FVector& Point, double Reach) const;
    bool HasSupport(const FSurvivalBuildPlacement& Placement) const;
    bool IsFootprintClear(const FSurvivalBuildPlacement& Placement, bool bPlanning) const;

    void MoveForward(float Value);
    void MoveRight(float Value);
    enum class EStance : uint8 { Standing, Crouched, Prone };
    void StartJump();
    void StartSprint();
    void StopSprint();
    void ToggleCrouch();
    void ToggleProne();
    void SetStance(EStance NewStance);
    void UpdateMovementPresentation();
    void Interact();
    void ToggleBuildMode();
    void RotatePiece();
    void NextPiece();
    void PreviousPiece();
    void PlacePlan();
    void RemovePlan();
    void Dismantle();
    void ToggleView();
    void UpdateSurvivorAnimation();
    void PrimaryAction();
    void EquipSidearm();
    void UpdateSidearmView();
    void LoadSidearmViewmodel();
    void UpdateSidearmAnimation();
    void PlaySidearmAnimation(UAnimSequence* Sequence, bool bLoop);
    void StartReload();
    void StartDefense();
    void RestartDefense();

    EStance Stance = EStance::Standing;
    bool bSprintHeld = false;
    double BodyMeshBottom = 0.0;
    FSurvivalMaterials Inventory;
    int32 SelectedPiece = 0;
    int32 QuarterTurns = 0;
    bool bBuildMode = false;
    bool bThirdPerson = false;
    bool bHasSidearm = false;
    int32 Ammo = 0;
    int32 ReserveAmmo = 0;
    double ReloadCompleteAt = -1.0;
    double NextShotAt = 0.0;
    bool bViewmodelReady = false;
    int32 HealthPoints = 100;
    double HitMarkerUntil = -1.0;
    int32 DefenseWave = 0;
    TArray<TWeakObjectPtr<ASurvivalZombie>> Attackers;

    UPROPERTY(Transient)
    TObjectPtr<ASurvivalSidearm> Sidearm;

    UPROPERTY(VisibleAnywhere, Category = "Camera")
    TObjectPtr<USkeletalMeshComponent> SidearmViewmodel;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> ViewmodelIdle;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> ViewmodelWalk;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> ViewmodelFire;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> ViewmodelReload;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> ViewmodelReloadEmpty;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> CurrentViewmodelAnimation;

    UPROPERTY(VisibleAnywhere, Category = "Camera")
    TObjectPtr<UCameraComponent> FirstPersonCamera;

    UPROPERTY(VisibleAnywhere, Category = "Camera")
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, Category = "Camera")
    TObjectPtr<UCameraComponent> ThirdPersonCamera;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> IdleAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> WalkAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> RunAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> CrouchIdleAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> CrouchMoveAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> ProneIdleAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> ProneMoveAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> JumpAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> CurrentAnimation;
};
