#pragma once

#include "GameFramework/Character.h"
#include "Misc/Optional.h"
#include "SurvivalBuilding.h"
#include "SurvivalVitals.h"
#include "SurvivalProgression.h"
#include "SurvivalCharacter.generated.h"

class ASurvivalSupplyCrate;
class ASurvivalSidearm;
class ASurvivalZombie;
struct FSurvivalFinishedPiece;
class UAnimSequence;
class UCameraComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class USkeletalMeshComponent;
class UStaticMeshComponent;
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
    const FSurvivalVitals& GetVitals() const { return Vitals; }
    static constexpr int32 HotbarSize = 6;
    ESurvivalItem GetHotbarItem(int32 Slot) const { return Hotbar[Slot]; }
    int32 GetActiveHotbarSlot() const { return ActiveHotbarSlot; }
    int32 GetItemCount(ESurvivalItem Item) const;
    bool CanUseItem(ESurvivalItem Item) const;
    static ESurvivalItem TrainingBook(ETraining Entry);
    ESurvivalItem GetSelectedInventoryItem() const { return SelectedInventoryItem; }
    void SetSelectedInventoryItem(ESurvivalItem Item) { SelectedInventoryItem = Item; }
    void ChooseHotbarSlot(int32 Slot);
    void UseItem(ESurvivalItem Item);
    bool CanCraftRecipe(int32 RecipeIndex, int32 Quantity) const;
    void CraftRecipe(int32 RecipeIndex, int32 Quantity);
    void ToggleInventory();
    bool IsInventoryOpen() const { return bInventoryOpen; }
    bool IsBuilding() const { return bBuildMode; }
    int32 GetSelectedPiece() const { return GetPlayerBuildPieces()[SelectedPiece]; }
    TOptional<FSurvivalBuildPlacement> FindPlacement() const;
    bool CanFinish(const FSurvivalBuildPlacement& Placement) const;
    const FSurvivalFinishedPiece* FindFinishedPiece() const;
    void TakeSurvivalDamage(int32 Amount);
    int32 GetHealthPoints() const { return HealthPoints; }
    int32 GetAmmo() const { return Ammo; }
    int32 GetReserveAmmo() const { return Items.Rounds; }
    bool IsReloading() const { return ReloadCompleteAt >= 0.0; }
    bool IsAimingSidearm() const;
    bool IsAimingRifle() const;
    float GetScopeBlend() const
    {
        return !bThirdPerson && !bBuildMode && !bInventoryOpen && HealthPoints > 0 && IsRifleSelected() ? ScopeBlend : 0.0f;
    }
    UTextureRenderTarget2D* GetScopeTexture() const;
    bool HasSidearm() const { return bHasSidearm; }
    int32 GetLivingAttackers() const;
    int32 GetLivingNightHorde() const;
    bool HasFirstHordeStarted() const { return bFirstHordeStarted; }
    bool TryStartFirstHorde();
    int32 GetDefenseWave() const { return DefenseWave; }
    static constexpr int32 DefenseWaveCount = 3;
    bool IsRifleSelected() const { return Hotbar[ActiveHotbarSlot] == ESurvivalItem::Rifle; }
    bool HasRifle() const { return bHasRifle; }
    int32 GetRifleAmmo() const { return RifleAmmo; }
    int32 GetRifleReserve() const { return Items.RifleRounds; }
    bool IsRifleReloading() const { return RifleReloadAt >= 0.0; }
    bool ShowHitMarker() const;
    void NotifyRoundHit(bool bHitZombie, bool bRifleRound);
    const FSurvivalProgression& GetProgression() const { return Progression; }
    void BuyTraining(ETraining Entry, int32 TargetRank);

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
    void ChangeItemCount(ESurvivalItem Item, int32 Delta);
    void SetInventoryOpen(bool bOpen);
    void HotbarOne() { ChooseHotbarSlot(0); }
    void HotbarTwo() { ChooseHotbarSlot(1); }
    void HotbarThree() { ChooseHotbarSlot(2); }
    void HotbarFour() { ChooseHotbarSlot(3); }
    void HotbarFive() { ChooseHotbarSlot(4); }
    void HotbarSix() { ChooseHotbarSlot(5); }
    bool IsSidearmSelected() const { return Hotbar[ActiveHotbarSlot] == ESurvivalItem::Sidearm; }
    void UpdateVitals(float DeltaSeconds);
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
    bool FireRound(const FVector& Muzzle, double Range, bool bRifleRound);
    void FireRifle();
    void ReloadRifle();
    void UpdateRifle();
    void LoadRifleViewmodel();
    void UpdateRifleView();
    void UpdateSidearmView();
    void LoadSidearmViewmodel();
    void UpdateSidearmAnimation();
    void UpdateWeaponAim(float DeltaSeconds);
    void UpdateSidearmSightAlignment();
    void PressAim();
    void ReleaseAim();
    void PlaySidearmAnimation(UAnimSequence* Sequence, bool bLoop);
    void StartReload();
    void StartDefense();
    bool SpawnAttackers(int32 Count, TArray<TWeakObjectPtr<ASurvivalZombie>>& Group);
    void RestartDefense();

    EStance Stance = EStance::Standing;
    bool bSprintHeld = false;
    double BodyMeshBottom = 0.0;
    FSurvivalMaterials Inventory;
    int32 SelectedPiece = 0;
    int32 QuarterTurns = 0;
    bool bBuildMode = false;
    bool bThirdPerson = false;
    FSurvivalProgression Progression;
    float SprintPracticeTime = 0.0f;
    bool bHasRifle = false;
    int32 RifleAmmo = 0;
    double RifleReadyAt = 0.0;
    double RifleReloadAt = -1.0;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> RifleHold;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> RifleFire;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> RifleReload;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> CurrentRifleAnimation;

    UPROPERTY(VisibleAnywhere, Category = "Camera")
    TObjectPtr<USkeletalMeshComponent> RifleViewmodel;

    UPROPERTY(VisibleAnywhere, Category = "Weapons")
    TObjectPtr<UStaticMeshComponent> RifleWorldMesh;

    bool bHasSidearm = false;
    int32 Ammo = 0;
    FSurvivalItems Items;
    FSurvivalVitals Vitals;
    bool bInventoryOpen = false;
    int32 ActiveHotbarSlot = 0;
    ESurvivalItem SelectedInventoryItem = ESurvivalItem::Food;
    ESurvivalItem Hotbar[HotbarSize] = {
        ESurvivalItem::Sidearm, ESurvivalItem::Food, ESurvivalItem::Water,
        ESurvivalItem::Bandage, ESurvivalItem::Rifle, ESurvivalItem::None
    };
    double ReloadCompleteAt = -1.0;
    double NextShotAt = 0.0;
    bool bViewmodelReady = false;
    bool bAimHeld = false;
    float AimBlend = 0.0f;
    float ScopeBlend = 0.0f;
    FVector RifleHip = FVector::ZeroVector;
    FVector RifleAim = FVector::ZeroVector;
    FQuat SidearmHipRotation = FQuat::Identity;
    FQuat SidearmAimRotation = FQuat::Identity;

    UPROPERTY(VisibleAnywhere, Category = "Camera")
    TObjectPtr<USceneCaptureComponent2D> ScopeCapture;

    UPROPERTY(Transient)
    TObjectPtr<UTextureRenderTarget2D> ScopeTexture;
    FVector ViewmodelHip = FVector::ZeroVector;
    FVector ViewmodelAim = FVector::ZeroVector;
    int32 HealthPoints = 100;
    double HitMarkerUntil = -1.0;
    int32 DefenseWave = 0;
    TArray<TWeakObjectPtr<ASurvivalZombie>> Attackers;
    TArray<TWeakObjectPtr<ASurvivalZombie>> NightHorde;
    bool bFirstHordeStarted = false;

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
