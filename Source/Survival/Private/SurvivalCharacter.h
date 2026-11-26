#pragma once

#include "GameFramework/Character.h"
#include "Engine/NetSerialization.h"
#include "Misc/Optional.h"
#include "SurvivalBuilding.h"
#include "SurvivalVitals.h"
#include "SurvivalProgression.h"
#include "SurvivalSupplyCrate.h"
#include "SurvivalCharacter.generated.h"

class ASurvivalSidearm;
class ASurvivalZombie;
class ASurvivalWorldDoor;
struct FSurvivalFinishedPiece;
class UAnimSequence;
class UCameraComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class USpotLightComponent;
class USpringArmComponent;

UCLASS()
class ASurvivalCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ASurvivalCharacter();

    ASurvivalSupplyCrate* FindSupplyCrate(FVector* ImpactPoint = nullptr) const;
    ASurvivalWorldDoor* FindWorldDoor(FVector* ImpactPoint = nullptr) const;
    FSurvivalPlanTarget FindPlan() const;
    FSurvivalMaterials GetPlannedCost() const;
    const FSurvivalMaterials& GetInventory() const { return Inventory; }
    const FSurvivalVitals& GetVitals() const { return Vitals; }
    static constexpr int32 HotbarSize = 6;
    ESurvivalItem GetHotbarItem(int32 Slot) const { return Toolbelt[Slot].Item; }
    int32 GetToolbeltCount(int32 Slot) const { return Toolbelt[Slot].Count; }
    int32 GetToolbeltWeaponId(int32 Slot) const { return Toolbelt[Slot].WeaponId; }
    const TCHAR* GetInstanceLabel(ESurvivalItem Weapon, int32 Id) const;
    const TCHAR* GetWeaponLabel(ESurvivalItem Weapon) const;
    int32 GetWeaponQuality(int32 WeaponId) const;
    void SelectToolbeltItem(int32 Slot);
    void InspectWeapon(ESurvivalItem Weapon);
    int32 GetActiveHotbarSlot() const { return ActiveHotbarSlot; }
    int32 GetItemCount(ESurvivalItem Item) const;
    bool CanUseItem(ESurvivalItem Item) const;
    static ESurvivalItem TrainingBook(ETraining Entry);
    ESurvivalItem GetSelectedInventoryItem() const { return SelectedInventoryItem; }
    void SetSelectedInventoryItem(ESurvivalItem Item)
    {
        SelectedInventoryItem = Item;
        SelectedPackSlot = INDEX_NONE;
        SelectedWeaponId = INDEX_NONE;
    }
    int32 GetSelectedPackSlot() const { return SelectedPackSlot; }
    void SetSelectedPackSlot(int32 Slot);
    static constexpr int32 PackSize = 24;
    static constexpr int32 PackRowWidth = 6;
    static constexpr int32 MaxPackCells = 96;
    int32 GetPackSlotCount() const { return Pack.Num(); }
    FSurvivalPackCell GetPackCell(int32 Slot) const { return Pack.IsValidIndex(Slot) ? Pack[Slot] : FSurvivalPackCell(); }
    ESurvivalItem GetPackItem(int32 Slot) const
    {
        return Pack.IsValidIndex(Slot) && !Pack[Slot].IsEmpty() ? Pack[Slot].Item : ESurvivalItem::None;
    }
    bool MoveMenuItem(bool bFromBelt, int32 From, bool bToBelt, int32 To);
    bool MovePackStack(int32 From, int32 To, bool bHalf);
    bool CanModifyRifle() const;
    bool SwapWeaponPart(int32 PackSlot);
    bool RemoveWeaponPart(int32 PartSlot);
    const FSurvivalWeaponAssembly& GetWeaponAssembly(ESurvivalItem Weapon) const;
    void ChooseHotbarSlot(int32 Slot);
    void UseToolbeltSlot(int32 Slot);
    void UseItem(ESurvivalItem Item);
    bool CanCraftRecipe(int32 RecipeIndex, int32 Quantity) const;
    void CraftRecipe(int32 RecipeIndex, int32 Quantity);
    void ToggleInventory();
    const FSurvivalLootSlot* GetContainerSlot(int32 Index) const;
    int32 GetContainerSlotCount() const { return ContainerSlots.Num(); }
    int32 GetContainerRevision() const { return ContainerRevision; }
    bool HasOpenContainer() const { return LootContainer.IsValid(); }
    float GetSearchSecondsLeft() const;
    void CloseContainer();
    void TakeContainerLoot(int32 Index = INDEX_NONE, int32 PackTarget = INDEX_NONE);
    void MoveContainerLoot(int32 From, int32 To, bool bSplit);
    UTextureRenderTarget2D* GetInventoryPortrait() const { return PortraitTexture; }
    void ShowWeaponPortrait(bool bWeapon, bool bCharacter = false);
    void RotateCharacterPortrait(float Degrees);
    bool bCharacterPortrait = false;
    bool CanInspectWeapon(ESurvivalItem Item) const;
    bool HasWeaponMod(ESurvivalItem Weapon, int32 Mod) const;
    bool CanToggleWeaponMod(ESurvivalItem Weapon, int32 Mod) const;
    void ToggleWeaponMod(ESurvivalItem Weapon, int32 Mod);
    ESurvivalItem InspectedWeapon = ESurvivalItem::None;
private:
    struct FWeaponInstance
    {
        ESurvivalItem Item;
        FSurvivalWeaponAssembly Assembly;
        int32 Ammo = 0;
        uint32 Mods = 0;
        int32 Firearm = INDEX_NONE;
    };
    TArray<FWeaponInstance> WeaponItems;
    int32 SelectedWeaponId = INDEX_NONE;
    int32 InspectedWeaponId = INDEX_NONE;
    int32 GetWeaponId(ESurvivalItem Weapon) const;
    void CancelWeaponReloadOnSwitch(int32 PreviousWeaponId);
    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> AttachmentMeshes;
    UPROPERTY()
    TArray<TObjectPtr<USpotLightComponent>> AttachmentLights;
    bool bHighOpticZoom = false;
    void RefreshAttachments();
    void UpdateFirearmModel();
    int32 EquippedFirearm = INDEX_NONE;
    float RifleADS = 0;
    void PlaySecondaryClip(const TCHAR* ClipRole, float Start, float Rate);
    float ActionDuration(bool bReload, UAnimSequence* Clip, float Fallback) const;
    int32 WeaponCapacity(ESurvivalItem Family) const;
    UPROPERTY()
    TObjectPtr<USkeletalMeshComponent> SecondaryRig;
    float WeaponRecoil(ESurvivalItem Weapon) const;
public:
    int32 GearCount(int32 Index) const;
    bool IsWearing(int32 Index) const { return WornGear.Contains(Index); }
    UFUNCTION(Server, Reliable)
    void ServerSetGearEquipped(int32 Index, bool bEquip);
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    static bool CanQuickSlot(ESurvivalItem Item);
    bool SplitSelection();
    bool CanDropSelection() const;
    void DropSelection();
    UFUNCTION(Server, Reliable)
    void ServerDropGear(int32 Index);
private:
    UPROPERTY(Replicated)
    TArray<int32> OwnedGear;
    UPROPERTY(ReplicatedUsing=RefreshGear)
    TArray<int32> WornGear;
    UFUNCTION()
    void RefreshGear();
    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> GearMeshes;
    UPROPERTY()
    TObjectPtr<USceneCaptureComponent2D> PortraitCapture;
    UPROPERTY()
    TObjectPtr<UTextureRenderTarget2D> PortraitTexture;
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> InspectionWeapon;
public:
    bool IsInventoryOpen() const { return bInventoryOpen; }
    bool IsDevSpeedActive() const { return bDevSpeedActive; }
    bool IsBuilding() const { return bBuildMode; }
    int32 GetSelectedPiece() const { return GetPlayerBuildPieces()[SelectedPiece]; }
    TOptional<FSurvivalBuildPlacement> FindPlacement() const;
    bool CanFinish(const FSurvivalBuildPlacement& Placement) const;
    const FSurvivalFinishedPiece* FindFinishedPiece() const;
    void TakeSurvivalDamage(int32 Amount);
    int32 GetHealthPoints() const { return HealthPoints; }
    int32 GetAmmo() const;
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
    bool IsRifleSelected() const { return GetHotbarItem(ActiveHotbarSlot) == ESurvivalItem::Rifle; }
    bool HasRifle() const { return bHasRifle; }
    int32 GetRifleAmmo() const;
    int32 GetRifleReserve() const { return Items.RifleRounds; }
    bool IsRifleReloading() const { return RifleReloadAt >= 0.0; }
    bool ShowHitMarker() const;
    void NotifyRoundHit(bool bHitZombie, bool bRifleRound);
    const FSurvivalProgression& GetProgression() const { return Progression; }
    int32 GetLootGameStage() const { return LootGameStage; }
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
    void ToggleDevSpeed();
    void ToggleCrouch();
    void ToggleProne();
    void SetStance(EStance NewStance);
    void UpdateMovementPresentation();
    void Interact();

    UFUNCTION(Server, Reliable)
    void ServerUseRoofLadder(AActor* Ladder, FVector_NetQuantize10 ImpactPoint);
    UFUNCTION(Server, Reliable)
    void ServerToggleWorldDoor(ASurvivalWorldDoor* Door, FVector_NetQuantize10 ImpactPoint);
    UFUNCTION(Server, Reliable)
    void ServerScavenge(ASurvivalSupplyCrate* Crate, FVector_NetQuantize10 ImpactPoint);

    UFUNCTION(Client, Reliable)
    void ClientReceiveLoot(const TArray<FSurvivalLootSlot>& Taken, int32 PackTarget, bool bApplyAppraisal);
    UFUNCTION(Client, Reliable)
    void ClientShowContainer(ASurvivalSupplyCrate* Crate, const TArray<FSurvivalLootSlot>& Slots,
        int32 Revision, bool bOpenMenu);
    UFUNCTION(Client, Reliable)
    void ClientSearchTime(float Seconds, bool bFinished);
    UFUNCTION(Server, Reliable)
    void ServerCloseContainer();
    UFUNCTION(Server, Reliable)
    void ServerTakeContainerLoot(ASurvivalSupplyCrate* Crate, int32 Revision,
        const TArray<int32>& Indices, int32 PackTarget);
    UFUNCTION(Server, Reliable)
    void ServerMoveContainerLoot(ASurvivalSupplyCrate* Crate, int32 Revision, int32 From, int32 To, bool bSplit);
    bool CanReachLoot(const ASurvivalSupplyCrate* Crate, FVector ImpactPoint) const;
    bool ValidateContainer(ASurvivalSupplyCrate* Crate, int32 Revision);
    void UpdateContainerSearch(float DeltaSeconds);
    void ShowContainer(ASurvivalSupplyCrate& Crate, bool bOpenMenu);

    struct FSearchProgress { int32 Seed = 0; float SecondsLeft = 0; };
    TMap<FName, FSearchProgress> SearchProgress;
    TWeakObjectPtr<ASurvivalSupplyCrate> SearchingContainer;
    TWeakObjectPtr<ASurvivalSupplyCrate> ServerContainer;
    FVector SearchImpactPoint = FVector::ZeroVector;
    FName ActiveSearchId;
    bool bSearchingContainer = false;
    bool bViewingContainer = false;
    int32 ServerSeenLootRevision = 0;
    double SearchEndsAt = 0;
    TWeakObjectPtr<ASurvivalSupplyCrate> LootContainer;
    TArray<FSurvivalLootSlot> ContainerSlots;
    int32 ContainerRevision = 0;
    TArray<FSurvivalPackCell> PendingLootItems;
    TMap<int32, int32> PendingGearTargets;

    void ChangeItemCount(ESurvivalItem Item, int32 Delta);
    void SetInventoryOpen(bool bOpen);
    void SyncPack();
    int32 FindFreePackCell();
    void AddToPack(ESurvivalItem Item, int32 Amount, int32 Quality);
    void RemoveFromPack(ESurvivalItem Item, int32 Amount);
    void HotbarOne() { ChooseHotbarSlot(0); }
    void HotbarTwo() { ChooseHotbarSlot(1); }
    void HotbarThree() { ChooseHotbarSlot(2); }
    void HotbarFour() { ChooseHotbarSlot(3); }
    void HotbarFive() { ChooseHotbarSlot(4); }
    void HotbarSix() { ChooseHotbarSlot(5); }
    bool IsSidearmSelected() const { return GetHotbarItem(ActiveHotbarSlot) == ESurvivalItem::Sidearm; }
    void UpdateVitals(float DeltaSeconds);
    void ToggleBuildMode();
    void RotatePiece();
    void NextPiece();
    void PreviousPiece();
    void PlacePlan();
    void RemovePlan();
    void Dismantle();
    void ToggleView();
    void UpdateSurvivorAnimation(float DeltaSeconds);
    void RefreshViewmodelGear(USkeletalMeshComponent* Viewmodel);
    float AirborneAnimationTime = 0.0f;
    bool bPlayingRun = false;
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
    bool bDevSpeedActive = false;
    float NormalAcceleration = 0.0f;
    double BodyMeshBottom = 0.0;
    FSurvivalMaterials Inventory;
    int32 SelectedPiece = 0;
    int32 QuarterTurns = 0;
    bool bBuildMode = false;
    bool bThirdPerson = false;
    FSurvivalProgression Progression;
    double SurvivalAgeSeconds = 0.0;
    UPROPERTY(Replicated)
    int32 LootGameStage = 1;
    float SprintPracticeTime = 0.0f;
    bool bHasRifle = false;
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
    FSurvivalItems Items;
    FSurvivalVitals Vitals;
    bool bInventoryOpen = false;
    TArray<FSurvivalPackCell> Pack;
    int32 SelectedPackSlot = INDEX_NONE;
    int32 ActiveHotbarSlot = 0;
    ESurvivalItem SelectedInventoryItem = ESurvivalItem::Food;
    FSurvivalPackCell Toolbelt[HotbarSize];
    float CharacterPortraitYaw = 0.0f;
    double ReloadCompleteAt = -1.0;
    double NextShotAt = 0.0;
    double SidearmAimedShotAt = -1.0;
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
