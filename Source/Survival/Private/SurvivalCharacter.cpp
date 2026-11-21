#include "SurvivalCharacter.h"

#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "SurvivalBuildChunk.h"
#include "SurvivalGameMode.h"
#include "SurvivalHUD.h"
#include "SurvivalRound.h"
#include "SurvivalSidearm.h"
#include "SurvivalSupplyCrate.h"
#include "SurvivalZombie.h"

namespace
{
constexpr double InteractReach = 260.0;
constexpr double BuildReach = 500.0;
constexpr double SupportTolerance = 8.0;
constexpr float WalkSpeed = 600.0f;
constexpr float SprintSpeed = 800.0f;
constexpr float CrouchSpeed = 280.0f;
constexpr float ProneSpeed = 120.0f;
constexpr float CrouchHalfHeight = 56.0f;
constexpr float ProneHalfHeight = 34.0f;
constexpr int32 SidearmCapacity = 12;
constexpr int32 RifleCapacity = 4;
const FName ViewmodelGripBone(TEXT("PBody_058"));
}

ASurvivalCharacter::ASurvivalCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);
    bUseControllerRotationYaw = true;
    GetCapsuleComponent()->InitCapsuleSize(ProneHalfHeight, 88.0f);
    GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
    GetCharacterMovement()->SetCrouchedHalfHeight(CrouchHalfHeight);
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
    GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed;

    FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
    FirstPersonCamera->SetRelativeLocation(FVector(0.0, 0.0, BaseEyeHeight));
    FirstPersonCamera->bUsePawnControlRotation = true;

    SidearmViewmodel = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SidearmViewmodel"));
    SidearmViewmodel->SetupAttachment(FirstPersonCamera);
    SidearmViewmodel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SidearmViewmodel->SetCastShadow(false);
    SidearmViewmodel->SetOnlyOwnerSee(true);
    SidearmViewmodel->SetHiddenInGame(true);
    SidearmViewmodel->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(GetCapsuleComponent());
    CameraBoom->SetRelativeLocation(FVector(0, 0, BaseEyeHeight));
    CameraBoom->TargetArmLength = 300.0;
    CameraBoom->SocketOffset = FVector(0, 55, 10);
    CameraBoom->bUsePawnControlRotation = true;

    ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCamera"));
    ThirdPersonCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    ThirdPersonCamera->SetAutoActivate(false);

    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->SetOwnerNoSee(true);
}

void ASurvivalCharacter::BeginPlay()
{
    Super::BeginPlay();
    USkeletalMesh* SurvivorMesh = LoadObject<USkeletalMesh>(nullptr,
        TEXT("/Game/Art/Crossroads/Player/SK_Survivor.SK_Survivor"));
    if (!SurvivorMesh)
    {
        return;
    }

    USkeletalMeshComponent* Body = GetMesh();
    USkeletalMesh* ArmedMesh = LoadObject<USkeletalMesh>(nullptr,
        TEXT("/Game/Art/Crossroads/ArmedPlayer/SK_ArmedSurvivor.SK_ArmedSurvivor"), nullptr, LOAD_NoWarn);
    Body->SetSkeletalMesh(ArmedMesh ? ArmedMesh : SurvivorMesh);
    const FBoxSphereBounds Bounds = SurvivorMesh->GetBounds();
    const double Scale = 175.0 / FMath::Max(1.0, 2.0 * Bounds.BoxExtent.Z);
    BodyMeshBottom = (Bounds.Origin.Z - Bounds.BoxExtent.Z) * Scale;
    Body->SetRelativeScale3D(FVector(Scale));
    Body->SetRelativeRotation(FRotator(0, -90, 0));
    UpdateMovementPresentation();

    const auto LoadClip = [ArmedMesh](const TCHAR* Name)
    {
        const TCHAR* Directory = ArmedMesh ? TEXT("ArmedPlayer") : TEXT("Player");
        const FString Path = FString::Printf(TEXT("/Game/Art/Crossroads/%s/%s.%s"), Directory, Name, Name);
        return LoadObject<UAnimSequence>(nullptr, *Path);
    };
    IdleAnimation = LoadClip(TEXT("SurvivorIdle"));
    WalkAnimation = LoadClip(TEXT("SurvivorWalk"));
    RunAnimation = LoadClip(TEXT("SurvivorRun"));
    JumpAnimation = LoadClip(TEXT("SurvivorJump"));
    CrouchIdleAnimation = LoadClip(TEXT("SurvivorCrouchIdle"));
    CrouchMoveAnimation = LoadClip(TEXT("SurvivorCrouchMove"));
    ProneIdleAnimation = LoadClip(TEXT("SurvivorProneIdle"));
    ProneMoveAnimation = LoadClip(TEXT("SurvivorProneMove"));
    if (ArmedMesh)
    {
        RifleHold = LoadClip(TEXT("SurvivorRifleHold"));
        RifleFire = LoadClip(TEXT("SurvivorRifleFire"));
        RifleReload = LoadClip(TEXT("SurvivorRifleReload"));
    }
    UpdateRifleVisibility();
    UpdateSurvivorAnimation();
}

void ASurvivalCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateVitals(DeltaSeconds);
    UpdateRifle();
    UpdateSurvivorAnimation();
    if (bHasSidearm && HealthPoints > 0)
    {
        UpdateSidearmAnimation();
    }
    UpdateSidearmAim(DeltaSeconds);
}

void ASurvivalCharacter::UpdateSurvivorAnimation()
{
    const bool bMoving = GetVelocity().SizeSquared2D() > FMath::Square(20.0);
    UAnimSequence* Desired = IdleAnimation.Get();
    if (Stance == EStance::Prone)
    {
        Desired = bMoving ? ProneMoveAnimation.Get() : ProneIdleAnimation.Get();
    }
    else if (bIsCrouched)
    {
        Desired = bMoving ? CrouchMoveAnimation.Get() : CrouchIdleAnimation.Get();
    }
    else if (GetCharacterMovement()->IsFalling() && JumpAnimation)
    {
        Desired = JumpAnimation.Get();
    }
    else if (bMoving)
    {
        Desired = GetVelocity().SizeSquared2D() > FMath::Square(WalkSpeed)
            ? RunAnimation.Get() : WalkAnimation.Get();
    }
    bool bLoop = Desired != JumpAnimation.Get();
    if (IsRifleSelected() && bHasRifle && !bBuildMode && !bInventoryOpen && HealthPoints > 0 && RifleHold)
    {
        const double Now = GetWorld()->GetTimeSeconds();
        Desired = RifleHold.Get();
        if (RifleReloadAt >= 0.0 && RifleReload)
        {
            Desired = RifleReload.Get();
        }
        else if (Now < RifleReadyAt && RifleFire)
        {
            Desired = RifleFire.Get();
        }
        bLoop = Desired == RifleHold.Get();
    }
    if (Desired && Desired != CurrentAnimation.Get())
    {
        CurrentAnimation = Desired;
        GetMesh()->PlayAnimation(Desired, bLoop);
    }
}

void ASurvivalCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &ASurvivalCharacter::MoveForward);
    PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &ASurvivalCharacter::MoveRight);
    PlayerInputComponent->BindAxis(TEXT("Turn"), this, &APawn::AddControllerYawInput);
    PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &APawn::AddControllerPitchInput);
    PlayerInputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &ASurvivalCharacter::StartJump);
    PlayerInputComponent->BindAction(TEXT("Jump"), IE_Released, this, &ACharacter::StopJumping);
    PlayerInputComponent->BindAction(TEXT("Sprint"), IE_Pressed, this, &ASurvivalCharacter::StartSprint);
    PlayerInputComponent->BindAction(TEXT("Sprint"), IE_Released, this, &ASurvivalCharacter::StopSprint);
    PlayerInputComponent->BindAction(TEXT("Crouch"), IE_Pressed, this, &ASurvivalCharacter::ToggleCrouch);
    PlayerInputComponent->BindAction(TEXT("Prone"), IE_Pressed, this, &ASurvivalCharacter::ToggleProne);
    PlayerInputComponent->BindAction(TEXT("Interact"), IE_Pressed, this, &ASurvivalCharacter::Interact);
    PlayerInputComponent->BindAction(TEXT("Inventory"), IE_Pressed, this, &ASurvivalCharacter::ToggleInventory);
    PlayerInputComponent->BindAction(TEXT("HotbarOne"), IE_Pressed, this, &ASurvivalCharacter::HotbarOne);
    PlayerInputComponent->BindAction(TEXT("HotbarTwo"), IE_Pressed, this, &ASurvivalCharacter::HotbarTwo);
    PlayerInputComponent->BindAction(TEXT("HotbarThree"), IE_Pressed, this, &ASurvivalCharacter::HotbarThree);
    PlayerInputComponent->BindAction(TEXT("HotbarFour"), IE_Pressed, this, &ASurvivalCharacter::HotbarFour);
    PlayerInputComponent->BindAction(TEXT("HotbarFive"), IE_Pressed, this, &ASurvivalCharacter::HotbarFive);
    PlayerInputComponent->BindAction(TEXT("HotbarSix"), IE_Pressed, this, &ASurvivalCharacter::HotbarSix);
    PlayerInputComponent->BindAction(TEXT("ToggleBuildMode"), IE_Pressed, this, &ASurvivalCharacter::ToggleBuildMode);
    PlayerInputComponent->BindAction(TEXT("RotatePiece"), IE_Pressed, this, &ASurvivalCharacter::RotatePiece);
    PlayerInputComponent->BindAction(TEXT("NextPiece"), IE_Pressed, this, &ASurvivalCharacter::NextPiece);
    PlayerInputComponent->BindAction(TEXT("PreviousPiece"), IE_Pressed, this, &ASurvivalCharacter::PreviousPiece);
    PlayerInputComponent->BindAction(TEXT("AimSidearm"), IE_Pressed, this, &ASurvivalCharacter::PressAim);
    PlayerInputComponent->BindAction(TEXT("AimSidearm"), IE_Released, this, &ASurvivalCharacter::ReleaseAim);
    PlayerInputComponent->BindAction(TEXT("PrimaryAction"), IE_Pressed, this, &ASurvivalCharacter::PrimaryAction);
    PlayerInputComponent->BindAction(TEXT("RemovePlan"), IE_Pressed, this, &ASurvivalCharacter::RemovePlan);
    PlayerInputComponent->BindAction(TEXT("Dismantle"), IE_Pressed, this, &ASurvivalCharacter::Dismantle);
    PlayerInputComponent->BindAction(TEXT("ToggleView"), IE_Pressed, this, &ASurvivalCharacter::ToggleView);
    PlayerInputComponent->BindAction(TEXT("StartDefense"), IE_Pressed, this, &ASurvivalCharacter::StartDefense);
    PlayerInputComponent->BindAction(TEXT("RestartDefense"), IE_Pressed, this, &ASurvivalCharacter::RestartDefense);
}

void ASurvivalCharacter::MoveForward(float Value)
{
    AddMovementInput(GetActorForwardVector(), Value);
}

void ASurvivalCharacter::MoveRight(float Value)
{
    AddMovementInput(GetActorRightVector(), Value);
}

void ASurvivalCharacter::StartJump()
{
    if (!bIsCrouched && !bInventoryOpen)
    {
        Jump();
    }
}

void ASurvivalCharacter::StartSprint()
{
    bSprintHeld = true;
}

void ASurvivalCharacter::StopSprint()
{
    bSprintHeld = false;
}

void ASurvivalCharacter::ToggleCrouch()
{
    SetStance(Stance == EStance::Crouched ? EStance::Standing : EStance::Crouched);
}

void ASurvivalCharacter::ToggleProne()
{
    SetStance(Stance == EStance::Prone ? EStance::Standing : EStance::Prone);
}

void ASurvivalCharacter::SetStance(EStance NewStance)
{
    UCharacterMovementComponent* Movement = GetCharacterMovement();
    if (HealthPoints <= 0 || bInventoryOpen || !Movement->IsMovingOnGround())
    {
        return;
    }
    if (NewStance == EStance::Standing)
    {
        Movement->UnCrouch(false);
        if (bIsCrouched)
        {
            return;
        }
        Movement->bWantsToCrouch = false;
    }
    else
    {
        const float DesiredHeight = NewStance == EStance::Prone ? ProneHalfHeight : CrouchHalfHeight;
        Movement->SetCrouchedHalfHeight(DesiredHeight);
        Movement->Crouch(false);
        if (!FMath::IsNearlyEqual(GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), DesiredHeight))
        {
            Movement->SetCrouchedHalfHeight(Stance == EStance::Prone ? ProneHalfHeight : CrouchHalfHeight);
            return;
        }
        Movement->bWantsToCrouch = true;
    }
    Stance = NewStance;
    UpdateMovementPresentation();
}

void ASurvivalCharacter::UpdateMovementPresentation()
{
    UCharacterMovementComponent* Movement = GetCharacterMovement();
    Movement->MaxWalkSpeedCrouched = Stance == EStance::Prone ? ProneSpeed : CrouchSpeed;

    double EyeHeight = BaseEyeHeight;
    if (Stance == EStance::Crouched)
    {
        EyeHeight = 36.0;
    }
    else if (Stance == EStance::Prone)
    {
        EyeHeight = 4.0;
    }
    FirstPersonCamera->SetRelativeLocation(FVector(FirstPersonCamera->GetRelativeLocation().X, 0, EyeHeight));
    CameraBoom->SetRelativeLocation(FVector(0, 0, EyeHeight));
    GetMesh()->SetRelativeLocation(FVector(0, 0, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - BodyMeshBottom));
}

void ASurvivalCharacter::ToggleView()
{
    bThirdPerson = !bThirdPerson;
    FirstPersonCamera->SetActive(!bThirdPerson);
    ThirdPersonCamera->SetActive(bThirdPerson);
    GetMesh()->SetOwnerNoSee(!bThirdPerson);
    UpdateSidearmView();
}

const UCameraComponent& ASurvivalCharacter::GetAimCamera() const
{
    return bThirdPerson ? *ThirdPersonCamera : *FirstPersonCamera;
}

bool ASurvivalCharacter::FindWorldAimHit(double Reach, FHitResult& Hit) const
{
    const UCameraComponent& Camera = GetAimCamera();
    const FVector Start = Camera.GetComponentLocation();
    const double Distance = Reach + FVector::Dist(Start, FirstPersonCamera->GetComponentLocation());
    return GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Camera.GetForwardVector() * Distance,
        ECC_Visibility, FCollisionQueryParams(SCENE_QUERY_STAT(CrossroadsAim), false, this));
}

bool ASurvivalCharacter::HasReachTo(const FVector& Point, double Reach) const
{
    const FVector Eyes = FirstPersonCamera->GetComponentLocation();
    if (FVector::DistSquared(Eyes, Point) > FMath::Square(Reach))
    {
        return false;
    }

    // Stop just shy of the target surface so a crate does not occlude itself.
    const FVector End = Point - (Point - Eyes).GetSafeNormal() * 0.5;
    return !GetWorld()->LineTraceTestByChannel(Eyes, End, ECC_Visibility,
        FCollisionQueryParams(SCENE_QUERY_STAT(CrossroadsReach), false, this));
}

ASurvivalSupplyCrate* ASurvivalCharacter::FindSupplyCrate() const
{
    FHitResult Hit;
    if (FindWorldAimHit(InteractReach, Hit) && HasReachTo(Hit.ImpactPoint, InteractReach))
    {
        return Cast<ASurvivalSupplyCrate>(Hit.GetActor());
    }
    return nullptr;
}

bool ASurvivalCharacter::FindPlanAimHit(double Reach, FHitResult& Hit) const
{
    const UCameraComponent& Camera = GetAimCamera();
    const FVector Start = Camera.GetComponentLocation();
    FHitResult Blocker;
    const FVector End = FindWorldAimHit(Reach, Blocker) ? Blocker.ImpactPoint :
        Start + Camera.GetForwardVector() *
        (Reach + FVector::Dist(Start, FirstPersonCamera->GetComponentLocation()));

    return GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_GameTraceChannel1,
        FCollisionQueryParams(SCENE_QUERY_STAT(SelectBuildPlan), false, this))
        && HasReachTo(Hit.ImpactPoint, Reach);
}

FSurvivalPlanTarget ASurvivalCharacter::FindPlan() const
{
    FHitResult Hit;
    if (FindPlanAimHit(InteractReach, Hit))
    {
        if (ASurvivalBuildChunk* Chunk = Cast<ASurvivalBuildChunk>(Hit.GetActor()))
        {
            return Chunk->GetPlanTarget(Hit);
        }
    }
    return {};
}

FSurvivalMaterials ASurvivalCharacter::GetPlannedCost() const
{
    const ASurvivalGameMode* GameMode = GetWorld()->GetAuthGameMode<ASurvivalGameMode>();
    return GameMode ? GameMode->GetPlannedCost() : FSurvivalMaterials{};
}

void ASurvivalCharacter::Interact()
{
    if (HealthPoints <= 0 || bInventoryOpen)
    {
        return;
    }
    const FSurvivalPlanTarget Target = FindPlan();
    if (const FSurvivalBuildPlacement* Plan = Target.GetPlan())
    {
        if (ASurvivalGameMode* GameMode = GetWorld()->GetAuthGameMode<ASurvivalGameMode>();
            GameMode && CanFinish(*Plan))
        {
            if (GameMode->FinishPlan(Target, Inventory))
            {
                Progression.AwardPractice(ESurvivalSkill::Building, 12);
            }
        }
    }
    else if (ASurvivalSupplyCrate* Crate = FindSupplyCrate())
    {
        const FSurvivalLoot Loot = Crate->TakeLoot();
        Inventory.Add(Loot.Materials);
        Items.Add(Loot.Items);
        if (Loot.bRifle && !bHasRifle)
        {
            bHasRifle = true;
            RifleAmmo = 1;
            UpdateRifleVisibility();
        }
        if (Loot.bSidearm && !bHasSidearm)
        {
            EquipSidearm();
        }
    }
    else
    {
        FHitResult Hit;
        if (FindWorldAimHit(InteractReach, Hit) && HasReachTo(Hit.ImpactPoint, InteractReach))
        {
            if (ASurvivalBuildChunk* Chunk = Cast<ASurvivalBuildChunk>(Hit.GetActor()))
            {
                if (const FSurvivalFinishedPiece* Piece = Chunk->FindFinished(Hit);
                    !bBuildMode && Piece && Piece->Placement.GetPiece().bDoorFrame)
                {
                    if (Piece->Door.IsValid())
                    {
                        if (Chunk->TryToggleDoor(Hit))
                        {
                            return;
                        }
                    }
                    else if (Items.TimberDoors > 0 && Chunk->TryInstallDoor(Hit))
                    {
                        --Items.TimberDoors;
                        return;
                    }
                    if (APlayerController* Player = Cast<APlayerController>(GetController()))
                    {
                        Player->ClientMessage(!Piece->Door.IsValid() && Items.TimberDoors == 0
                            ? TEXT("Craft a timber door first.") : TEXT("Doorway is blocked."));
                    }
                }
                else
                {
                    const int32 RepairHealth = Progression.HasPerk(ESurvivalSkill::Building) ? 45 : 30;
                    Chunk->RepairFinished(Hit, Inventory, RepairHealth);
                }
            }
        }
    }
}

const FSurvivalFinishedPiece* ASurvivalCharacter::FindFinishedPiece() const
{
    FHitResult Hit;
    if (FindWorldAimHit(InteractReach, Hit) && HasReachTo(Hit.ImpactPoint, InteractReach))
    {
        if (const ASurvivalBuildChunk* Chunk = Cast<ASurvivalBuildChunk>(Hit.GetActor()))
        {
            return Chunk->FindFinished(Hit);
        }
    }
    return nullptr;
}

void ASurvivalCharacter::EquipSidearm()
{
    bHasSidearm = true;
    Ammo = SidearmCapacity;
    Items.Rounds += SidearmCapacity;
    Sidearm = GetWorld()->SpawnActor<ASurvivalSidearm>();
    LoadSidearmViewmodel();
    UpdateSidearmView();
}

void ASurvivalCharacter::UpdateSidearmView()
{
    UpdateRifleVisibility();
    const bool bVisible = IsSidearmSelected() && !bBuildMode && !bInventoryOpen && HealthPoints > 0;
    SidearmViewmodel->SetHiddenInGame(!bVisible || bThirdPerson || !bViewmodelReady);
    if (!Sidearm)
    {
        return;
    }

    const FName Hand(TEXT("RightHand"));
    if (bThirdPerson)
    {
        const bool bHandAvailable = GetMesh()->DoesSocketExist(Hand);
        Sidearm->SetActorHiddenInGame(!bVisible || !bHandAvailable);
        if (bHandAvailable)
        {
            Sidearm->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Hand);
            Sidearm->SetActorRelativeRotation(FRotator(0, 0, 90));
        }
    }
    else
    {
        Sidearm->SetActorHiddenInGame(!bVisible || bViewmodelReady);
        if (bViewmodelReady)
        {
            Sidearm->AttachToComponent(SidearmViewmodel, FAttachmentTransformRules::SnapToTargetNotIncludingScale,
                ViewmodelGripBone);
        }
        else
        {
            Sidearm->AttachToComponent(FirstPersonCamera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
            Sidearm->SetActorRelativeLocation(FVector(48, 19, -34));
        }
    }
}

void ASurvivalCharacter::LoadSidearmViewmodel()
{
    constexpr TCHAR MeshPath[] = TEXT("/Game/Art/Crossroads/MagazineViewmodel/SK_SidearmViewmodel.SK_SidearmViewmodel");
    USkeletalMesh* Arms = LoadObject<USkeletalMesh>(nullptr, MeshPath, nullptr, LOAD_NoWarn);
    if (!Arms)
    {
        return;
    }

    SidearmViewmodel->SetSkeletalMesh(Arms);
    const FName CameraBone(TEXT("Head_Cam_014"));
    if (!SidearmViewmodel->DoesSocketExist(CameraBone) || !SidearmViewmodel->DoesSocketExist(ViewmodelGripBone))
    {
        return;
    }
    SidearmViewmodel->RefreshBoneTransforms(nullptr);
    const FTransform Eye = SidearmViewmodel->GetSocketTransform(CameraBone, RTS_Component);
    const FTransform Grip = SidearmViewmodel->GetSocketTransform(ViewmodelGripBone, RTS_Component);
    const FVector Reach = Grip.GetLocation() - Eye.GetLocation();
    if (Reach.Size2D() < UE_KINDA_SMALL_NUMBER)
    {
        return;
    }
    // Select the grip axis that points out from the authored camera, regardless of the imported basis.
    const FVector EyeToGrip = Reach.GetSafeNormal();
    FVector Forward = EyeToGrip;
    double BestAlignment = 0.0;
    for (const FVector& Axis : {FVector::ForwardVector, FVector::RightVector, FVector::UpVector})
    {
        const FVector Candidate = Grip.GetRotation().RotateVector(Axis);
        const double Alignment = FVector::DotProduct(Candidate, EyeToGrip);
        if (FMath::Abs(Alignment) > BestAlignment)
        {
            BestAlignment = FMath::Abs(Alignment);
            Forward = Candidate * FMath::Sign(Alignment);
        }
    }
    const FQuat CameraAlignment = FRotator(0, -Forward.Rotation().Yaw, 0).Quaternion();
    const double Scale = 43.0 / Reach.Size();
    SidearmViewmodel->SetRelativeScale3D(FVector(Scale));
    SidearmViewmodel->SetRelativeRotation(CameraAlignment);
    const FVector CameraOffset = CameraAlignment.RotateVector(Eye.GetLocation() * Scale);
    const FVector GripFromEye = CameraAlignment.RotateVector(Reach * Scale);
    ViewmodelHip = FVector(10, 0, -34) - CameraOffset;
    const FName SightBone(TEXT("FrontSight"));
    if (SidearmViewmodel->DoesSocketExist(SightBone))
    {
        const FVector Sight = SidearmViewmodel->GetSocketTransform(SightBone, RTS_Component).GetLocation();
        const FVector SightFromEye = CameraAlignment.RotateVector((Sight - Eye.GetLocation()) * Scale);
        ViewmodelAim = FVector(10, -SightFromEye.Y, -SightFromEye.Z) - CameraOffset;
    }
    else
    {
        ViewmodelAim = FVector(10, -GripFromEye.Y, -GripFromEye.Z - 11) - CameraOffset;
    }
    SidearmViewmodel->SetRelativeLocation(ViewmodelHip);

    const auto LoadClip = [](const TCHAR* Name)
    {
        const FString Path = FString::Printf(TEXT("/Game/Art/Crossroads/MagazineViewmodel/%s.%s"), Name, Name);
        return LoadObject<UAnimSequence>(nullptr, *Path);
    };
    ViewmodelIdle = LoadClip(TEXT("ViewmodelIdle"));
    ViewmodelWalk = LoadClip(TEXT("ViewmodelWalk"));
    ViewmodelFire = LoadClip(TEXT("ViewmodelFire"));
    ViewmodelReload = LoadClip(TEXT("ViewmodelReload"));
    ViewmodelReloadEmpty = LoadClip(TEXT("ViewmodelReloadEmpty"));
    bViewmodelReady = ViewmodelIdle != nullptr;
    UpdateSidearmAnimation();
}

void ASurvivalCharacter::PlaySidearmAnimation(UAnimSequence* Sequence, bool bLoop)
{
    if (bViewmodelReady && Sequence && (!bLoop || Sequence != CurrentViewmodelAnimation.Get()))
    {
        CurrentViewmodelAnimation = Sequence;
        SidearmViewmodel->PlayAnimation(Sequence, bLoop);
    }
}

void ASurvivalCharacter::UpdateSidearmAnimation()
{
    const double Now = GetWorld()->GetTimeSeconds();
    if (IsReloading() && Now >= ReloadCompleteAt)
    {
        const int32 Transfer = FMath::Min(Items.Rounds, SidearmCapacity - Ammo);
        Ammo += Transfer;
        Items.Rounds -= Transfer;
        ReloadCompleteAt = -1.0;
    }
    if (!IsReloading() && Now >= NextShotAt)
    {
        const bool bMoving = GetVelocity().SizeSquared2D() > FMath::Square(20.0);
        PlaySidearmAnimation(bMoving && !IsAimingSidearm() && ViewmodelWalk
            ? ViewmodelWalk.Get() : ViewmodelIdle.Get(), true);
    }
}

bool ASurvivalCharacter::IsAimingSidearm() const
{
    return bAimHeld && bHasSidearm && IsSidearmSelected() && !bThirdPerson && !bBuildMode && !bInventoryOpen && !IsReloading() && HealthPoints > 0;
}

void ASurvivalCharacter::PressAim()
{
    bAimHeld = true;
}

void ASurvivalCharacter::ReleaseAim()
{
    bAimHeld = false;
}

void ASurvivalCharacter::UpdateSidearmAim(float DeltaSeconds)
{
    const float Target = IsAimingSidearm() ? 1.0f : 0.0f;
    if (AimBlend == Target)
    {
        return;
    }
    AimBlend = FMath::FInterpConstantTo(AimBlend, Target, DeltaSeconds, 6.0f);
    FirstPersonCamera->SetFieldOfView(FMath::Lerp(90.0f, 65.0f, AimBlend));
    if (bViewmodelReady)
    {
        SidearmViewmodel->SetRelativeLocation(FMath::Lerp(ViewmodelHip, ViewmodelAim, AimBlend));
    }
    else if (Sidearm && !bThirdPerson)
    {
        Sidearm->SetActorRelativeLocation(FMath::Lerp(FVector(48, 19, -34), FVector(48, 0, -7.5), AimBlend));
    }
}

void ASurvivalCharacter::StartReload()
{
    const double Now = GetWorld()->GetTimeSeconds();
    if (HealthPoints <= 0 || bInventoryOpen || !bHasSidearm || !IsSidearmSelected() || IsReloading() || Now < NextShotAt
        || Ammo >= SidearmCapacity || Items.Rounds <= 0)
    {
        return;
    }
    UAnimSequence* Clip = Ammo == 0 && ViewmodelReloadEmpty ? ViewmodelReloadEmpty.Get() : ViewmodelReload.Get();
    if (!Clip)
    {
        Clip = ViewmodelReloadEmpty.Get();
    }
    ReloadCompleteAt = Now + (Clip ? Clip->GetPlayLength() : 1.4);
    PlaySidearmAnimation(Clip, false);
}

bool ASurvivalCharacter::ShowHitMarker() const
{
    return GetWorld()->GetTimeSeconds() < HitMarkerUntil;
}

void ASurvivalCharacter::PrimaryAction()
{
    if (HealthPoints <= 0 || bInventoryOpen)
    {
        return;
    }
    if (bBuildMode)
    {
        PlacePlan();
        return;
    }
    if (IsRifleSelected())
    {
        FireRifle();
        return;
    }
    if (!IsSidearmSelected())
    {
        UseItem(Hotbar[ActiveHotbarSlot]);
        return;
    }
    const double Now = GetWorld()->GetTimeSeconds();
    if (!bHasSidearm || Ammo <= 0 || IsReloading() || Now < NextShotAt)
    {
        return;
    }

    const FVector Muzzle = bThirdPerson && Sidearm && !Sidearm->IsHidden()
        ? Sidearm->GetActorTransform().TransformPosition(FVector(18, 0, 5))
        : FirstPersonCamera->GetComponentTransform().TransformPosition(
            FMath::Lerp(FVector(52, 9, -21), FVector(52, 0, -4), AimBlend));
    if (!FireRound(Muzzle, 5500.0, false))
    {
        return;
    }

    --Ammo;
    NextShotAt = Now + (ViewmodelFire ? ViewmodelFire->GetPlayLength() : 0.23);
    PlaySidearmAnimation(ViewmodelFire.Get(), false);
    if (Sidearm)
    {
        Sidearm->PlayShotSound();
    }
    AddControllerPitchInput(Progression.HasPerk(ESurvivalSkill::Marksmanship) ? -0.4875f : -0.65f);
}

bool ASurvivalCharacter::FireRound(const FVector& Origin, double Range, bool bRifleRound)
{
    const UCameraComponent& Camera = GetAimCamera();
    FHitResult Aim;
    const FVector Target = FindWorldAimHit(Range, Aim)
        ? Aim.ImpactPoint : Camera.GetComponentLocation() + Camera.GetForwardVector() * Range;
    const FVector Eyes = FirstPersonCamera->GetComponentLocation();
    FVector Muzzle = Origin;
    FHitResult Obstruction;
    if (GetWorld()->SweepSingleByChannel(Obstruction, Eyes, Muzzle, FQuat::Identity, ECC_Visibility,
        FCollisionShape::MakeSphere(1.0), FCollisionQueryParams(SCENE_QUERY_STAT(SidearmClearance), false, this))
        || FVector::DotProduct(Target - Muzzle, Camera.GetForwardVector()) <= 0.0)
    {
        Muzzle = Eyes;
    }
    const FVector Direction = (Target - Muzzle).GetSafeNormal();
    if (Direction.IsNearlyZero())
    {
        return false;
    }
    FActorSpawnParameters Parameters;
    Parameters.Owner = this;
    Parameters.Instigator = this;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ASurvivalRound* Round = GetWorld()->SpawnActor<ASurvivalRound>(Muzzle, Direction.Rotation(), Parameters);
    if (Round && bRifleRound)
    {
        Round->SetRifleRound();
    }
    return Round != nullptr;

}

void ASurvivalCharacter::UpdateRifleVisibility()
{
    const FName Weapon(TEXT("RifleMount"));
    const FName Head(TEXT("Head"));
    const bool bArmed = bHasRifle && IsRifleSelected() && !bBuildMode && !bInventoryOpen && HealthPoints > 0 && RifleHold;
    const bool bFirstPersonRifle = bArmed && !bThirdPerson;
    if (GetMesh()->GetBoneIndex(Weapon) != INDEX_NONE)
    {
        if (bArmed)
        {
            GetMesh()->UnHideBoneByName(Weapon);
        }
        else
        {
            GetMesh()->HideBoneByName(Weapon, PBO_None);
        }
    }
    if (GetMesh()->GetBoneIndex(Head) != INDEX_NONE)
    {
        if (bFirstPersonRifle)
        {
            GetMesh()->HideBoneByName(Head, PBO_None);
        }
        else
        {
            GetMesh()->UnHideBoneByName(Head);
        }
    }
    GetMesh()->SetOwnerNoSee(!bThirdPerson && !bFirstPersonRifle);
    FVector CameraPosition = FirstPersonCamera->GetRelativeLocation();
    CameraPosition.X = bFirstPersonRifle ? 20.0 : 0.0;
    FirstPersonCamera->SetRelativeLocation(CameraPosition);
}

void ASurvivalCharacter::FireRifle()
{
    const double Now = GetWorld()->GetTimeSeconds();
    if (!bHasRifle || RifleAmmo <= 0 || IsRifleReloading() || Now < RifleReadyAt)
    {
        return;
    }
    const FVector Origin = FirstPersonCamera->GetComponentTransform().TransformPosition(FVector(74, 0, -13));
    if (!FireRound(Origin, 12000.0, true))
    {
        return;
    }
    --RifleAmmo;
    CurrentAnimation = nullptr;
    RifleReadyAt = Now + (RifleFire ? RifleFire->GetPlayLength() : 2.0);
    AddControllerPitchInput(Progression.HasPerk(ESurvivalSkill::Marksmanship) ? -1.0125f : -1.35f);
    if (USoundBase* Sound = LoadObject<USoundBase>(nullptr,
        TEXT("/Game/Audio/Weapons/SW_SidearmShot.SW_SidearmShot"), nullptr, LOAD_NoWarn))
    {
        UGameplayStatics::PlaySoundAtLocation(this, Sound, Origin);
    }
}

void ASurvivalCharacter::ReloadRifle()
{
    const double Now = GetWorld()->GetTimeSeconds();
    if (bHasRifle && HealthPoints > 0 && !bInventoryOpen && !IsRifleReloading()
        && RifleAmmo < RifleCapacity && Items.RifleRounds > 0 && Now >= RifleReadyAt)
    {
        CurrentAnimation = nullptr;
        RifleReloadAt = Now + (RifleReload ? RifleReload->GetPlayLength() : 1.8);
    }
}

void ASurvivalCharacter::UpdateRifle()
{
    if (IsRifleReloading() && GetWorld()->GetTimeSeconds() >= RifleReloadAt)
    {
        const int32 Transfer = FMath::Min(RifleCapacity - RifleAmmo, Items.RifleRounds);
        Items.RifleRounds -= Transfer;
        RifleAmmo += Transfer;
        RifleReloadAt = -1.0;
    }
}

void ASurvivalCharacter::BuySkillPerk(ESurvivalSkill Skill)
{
    if (bInventoryOpen && HealthPoints > 0)
    {
        Progression.BuyPerk(Skill);
    }
}

void ASurvivalCharacter::NotifySidearmHit(bool bHitZombie)
{
    if (bHitZombie && HealthPoints > 0)
    {
        Progression.AwardPractice(ESurvivalSkill::Marksmanship, 10);
    }
    HitMarkerUntil = GetWorld()->GetTimeSeconds() + 0.15;
}

namespace
{
int32 CountLivingZombies(const TArray<TWeakObjectPtr<ASurvivalZombie>>& Group)
{
    int32 Count = 0;
    for (const TWeakObjectPtr<ASurvivalZombie>& Zombie : Group)
    {
        Count += Zombie.IsValid() ? 1 : 0;
    }
    return Count;
}
}

int32 ASurvivalCharacter::GetLivingAttackers() const
{
    return CountLivingZombies(Attackers);
}

int32 ASurvivalCharacter::GetLivingNightHorde() const
{
    return CountLivingZombies(NightHorde);
}

bool ASurvivalCharacter::TryStartFirstHorde()
{
    if (!bFirstHordeStarted && HealthPoints > 0 && GetLivingAttackers() == 0)
    {
        bFirstHordeStarted = SpawnAttackers(3, NightHorde);
    }
    return bFirstHordeStarted;
}

void ASurvivalCharacter::StartDefense()
{
    if (HealthPoints <= 0 || bInventoryOpen || DefenseWave >= DefenseWaveCount
        || GetLivingAttackers() > 0 || GetLivingNightHorde() > 0)
    {
        return;
    }

    const int32 NextWave = DefenseWave + 1;
    if (SpawnAttackers(NextWave, Attackers))
    {
        DefenseWave = NextWave;
        if (DefenseWave > 1 && bHasSidearm)
        {
            Items.Rounds += 6;
        }
    }
}

bool ASurvivalCharacter::SpawnAttackers(int32 Count, TArray<TWeakObjectPtr<ASurvivalZombie>>& Group)
{
    const FVector SpawnPoints[] = {
        FVector(3700.0, 1650.0, 112.0),
        FVector(3490.0, 1410.0, 112.0),
        FVector(3920.0, 1240.0, 112.0)
    };
    static_assert(UE_ARRAY_COUNT(SpawnPoints) == DefenseWaveCount);
    Group.Reset();
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        ASurvivalZombie* Zombie = GetWorld()->SpawnActor<ASurvivalZombie>(
            SpawnPoints[Index], FRotator(0, 90, 0), SpawnParameters);
        if (!Zombie)
        {
            for (const TWeakObjectPtr<ASurvivalZombie>& Spawned : Group)
            {
                if (ASurvivalZombie* PartialWave = Spawned.Get())
                {
                    PartialWave->Destroy();
                }
            }
            Group.Reset();
            return false;
        }
        Group.Add(Zombie);
    }

    return true;
}

void ASurvivalCharacter::TakeSurvivalDamage(int32 Amount)
{
    HealthPoints = FMath::Max(0, HealthPoints - Amount);
    if (HealthPoints == 0)
    {
        SetInventoryOpen(false);
        bBuildMode = false;
        ReloadCompleteAt = -1.0;
        RifleReloadAt = -1.0;
        UpdateSidearmView();
        GetCharacterMovement()->DisableMovement();
    }
}

void ASurvivalCharacter::RestartDefense()
{
    UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
}

void ASurvivalCharacter::UpdateVitals(float DeltaSeconds)
{
    if (HealthPoints <= 0)
    {
        return;
    }
    const bool bSprintAllowed = bSprintHeld && Stance == EStance::Standing && Vitals.CanSprint()
        && !IsAimingSidearm() && !bInventoryOpen;
    const bool bSprinting = bSprintAllowed && GetVelocity().SizeSquared2D() > FMath::Square(20.0);
    const ASurvivalGameMode* Mode = GetWorld()->GetAuthGameMode<ASurvivalGameMode>();
    const float Ambient = Mode ? static_cast<float>(Mode->GetAmbientTemperature()) : 14.0f;
    const float Effort = static_cast<float>(FMath::Clamp(GetVelocity().Size2D() / WalkSpeed, 0.0, 1.5));
    const float SprintCost = Progression.HasPerk(ESurvivalSkill::Endurance) ? 0.8f : 1.0f;
    const int32 Damage = Vitals.Advance(DeltaSeconds, bSprinting, Effort, Ambient, SprintCost);
    if (bSprinting)
    {
        constexpr float PracticeInterval = 8.0f;
        SprintPracticeTime += DeltaSeconds;
        const int32 Practices = FMath::FloorToInt(SprintPracticeTime / PracticeInterval);
        SprintPracticeTime -= Practices * PracticeInterval;
        Progression.AwardPractice(ESurvivalSkill::Endurance, Practices * 10);
    }
    GetCharacterMovement()->MaxWalkSpeed = bSprintAllowed && Vitals.CanSprint() ? SprintSpeed : WalkSpeed;
    if (Damage > 0)
    {
        TakeSurvivalDamage(Damage);
    }
}

void ASurvivalCharacter::ToggleInventory()
{
    if (HealthPoints > 0)
    {
        SetInventoryOpen(!bInventoryOpen);
    }
}

void ASurvivalCharacter::SetInventoryOpen(bool bOpen)
{
    if (bInventoryOpen == bOpen)
    {
        return;
    }
    bInventoryOpen = bOpen;
    bAimHeld = false;
    bSprintHeld = false;
    if (bOpen)
    {
        bBuildMode = false;
    }
    if (APlayerController* Player = Cast<APlayerController>(GetController()))
    {
        if (ASurvivalHUD* HUD = Cast<ASurvivalHUD>(Player->GetHUD()))
        {
            HUD->SetInventoryVisible(*this, bOpen);
        }
    }
    UpdateSidearmView();
}

int32 ASurvivalCharacter::GetItemCount(ESurvivalItem Item) const
{
    switch (Item)
    {
    case ESurvivalItem::Sidearm: return bHasSidearm ? 1 : 0;
    case ESurvivalItem::Food: return Items.Food;
    case ESurvivalItem::Water: return Items.Water;
    case ESurvivalItem::Bandage: return Items.Bandages;
    case ESurvivalItem::Timber: return Inventory.Timber;
    case ESurvivalItem::Stone: return Inventory.Stone;
    case ESurvivalItem::Cloth: return Items.Cloth;
    case ESurvivalItem::Rounds: return Items.Rounds;
    case ESurvivalItem::TimberDoor: return Items.TimberDoors;
    case ESurvivalItem::Rifle: return bHasRifle ? 1 : 0;
    case ESurvivalItem::RifleRounds: return Items.RifleRounds;
    case ESurvivalItem::BuilderGuide: return Items.BuilderGuides;
    default: return 0;
    }
}

bool ASurvivalCharacter::CanUseItem(ESurvivalItem Item) const
{
    if (HealthPoints <= 0 || GetItemCount(Item) <= 0)
    {
        return false;
    }
    switch (Item)
    {
    case ESurvivalItem::Food: return Vitals.Hunger < FSurvivalVitals::Max;
    case ESurvivalItem::Water: return Vitals.Thirst < FSurvivalVitals::Max;
    case ESurvivalItem::Bandage: return HealthPoints < 100;
    case ESurvivalItem::BuilderGuide: return Progression.Rank(ESurvivalSkill::Building) < FSurvivalProgression::MaxRank;
    default: return false;
    }
}

void ASurvivalCharacter::UseItem(ESurvivalItem Item)
{
    if (!CanUseItem(Item))
    {
        return;
    }
    switch (Item)
    {
    case ESurvivalItem::Food:
        --Items.Food;
        Vitals.Hunger = FMath::Min(FSurvivalVitals::Max, Vitals.Hunger + 34.0f);
        break;
    case ESurvivalItem::Water:
        --Items.Water;
        Vitals.Thirst = FMath::Min(FSurvivalVitals::Max, Vitals.Thirst + 42.0f);
        break;
    case ESurvivalItem::Bandage:
        --Items.Bandages;
        HealthPoints = FMath::Min(100, HealthPoints + 30);
        break;
    case ESurvivalItem::BuilderGuide:
        --Items.BuilderGuides;
        Progression.ReadGuide(ESurvivalSkill::Building);
        break;
    default:
        break;
    }
}

void ASurvivalCharacter::ChooseHotbarSlot(int32 Slot)
{
    if (Slot < 0 || Slot >= HotbarSize || HealthPoints <= 0)
    {
        return;
    }
    if (bInventoryOpen)
    {
        if (SelectedInventoryItem != ESurvivalItem::None && GetItemCount(SelectedInventoryItem) <= 0)
        {
            return;
        }
        for (int32 Index = 0; Index < HotbarSize; ++Index)
        {
            if (Index != Slot && Hotbar[Index] == SelectedInventoryItem && SelectedInventoryItem != ESurvivalItem::None)
            {
                Hotbar[Index] = Hotbar[Slot];
                break;
            }
        }
        Hotbar[Slot] = SelectedInventoryItem;
    }
    ActiveHotbarSlot = Slot;
    UpdateSidearmView();
}

bool ASurvivalCharacter::CanCraftRecipe(int32 RecipeIndex, int32 Quantity) const
{
    const TConstArrayView<FSurvivalRecipe> Recipes = GetSurvivalRecipes();
    if (!bInventoryOpen || HealthPoints <= 0 || !Recipes.IsValidIndex(RecipeIndex) || Quantity < 1 || Quantity > 99)
    {
        return false;
    }
    for (const FSurvivalIngredient& Ingredient : Recipes[RecipeIndex].Ingredients)
    {
        if (GetItemCount(Ingredient.Item) < Ingredient.Count * Quantity)
        {
            return false;
        }
    }
    return true;
}

void ASurvivalCharacter::CraftRecipe(int32 RecipeIndex, int32 Quantity)
{
    if (!CanCraftRecipe(RecipeIndex, Quantity))
    {
        return;
    }
    const FSurvivalRecipe& Recipe = GetSurvivalRecipes()[RecipeIndex];
    for (const FSurvivalIngredient& Ingredient : Recipe.Ingredients)
    {
        ChangeItemCount(Ingredient.Item, -Ingredient.Count * Quantity);
    }
    ChangeItemCount(Recipe.Output, Quantity);
}

void ASurvivalCharacter::ChangeItemCount(ESurvivalItem Item, int32 Delta)
{
    switch (Item)
    {
    case ESurvivalItem::Food: Items.Food += Delta; break;
    case ESurvivalItem::Water: Items.Water += Delta; break;
    case ESurvivalItem::Bandage: Items.Bandages += Delta; break;
    case ESurvivalItem::Timber: Inventory.Timber += Delta; break;
    case ESurvivalItem::Stone: Inventory.Stone += Delta; break;
    case ESurvivalItem::Cloth: Items.Cloth += Delta; break;
    case ESurvivalItem::Rounds: Items.Rounds += Delta; break;
    case ESurvivalItem::TimberDoor: Items.TimberDoors += Delta; break;
    default: checkNoEntry(); break;
    }
}

void ASurvivalCharacter::ToggleBuildMode()
{
    SetInventoryOpen(false);
    bBuildMode = HealthPoints > 0 && !bBuildMode;
    UpdateSidearmView();
}

void ASurvivalCharacter::RotatePiece()
{
    if (bBuildMode)
    {
        QuarterTurns = (QuarterTurns + 1) % 4;
    }
    else if (IsRifleSelected())
    {
        ReloadRifle();
    }
    else
    {
        StartReload();
    }
}

void ASurvivalCharacter::NextPiece()
{
    if (bBuildMode)
    {
        SelectedPiece = (SelectedPiece + 1) % GetPlayerBuildPieces().Num();
    }
    else if (!bInventoryOpen)
    {
        ChooseHotbarSlot((ActiveHotbarSlot + 1) % HotbarSize);
    }
}

void ASurvivalCharacter::PreviousPiece()
{
    if (bBuildMode)
    {
        const int32 Count = GetPlayerBuildPieces().Num();
        SelectedPiece = (SelectedPiece + Count - 1) % Count;
    }
    else if (!bInventoryOpen)
    {
        ChooseHotbarSlot((ActiveHotbarSlot + HotbarSize - 1) % HotbarSize);
    }
}

bool ASurvivalCharacter::HasSupport(const FSurvivalBuildPlacement& Placement) const
{
    const ASurvivalGameMode* GameMode = GetWorld()->GetAuthGameMode<ASurvivalGameMode>();
    return GameMode && GameMode->CanSupport(Placement);
}

bool ASurvivalCharacter::IsFootprintClear(const FSurvivalBuildPlacement& Placement, bool bPlanning) const
{
    FCollisionQueryParams Query(SCENE_QUERY_STAT(BuildOverlap), false);
    if (bPlanning)
    {
        Query.AddIgnoredActor(this);
    }

    if (const ASurvivalGameMode* Mode = GetWorld()->GetAuthGameMode<ASurvivalGameMode>();
        Mode && Mode->HasReservedCellConflict(Placement, bPlanning))
    {
        return false;
    }
    const auto IsClear = [&](const FBox& Box)
    {
        // Inset the query so touching faces and a door resting in its frame remain valid.
        const FCollisionShape Shape = FCollisionShape::MakeBox(Box.GetExtent() - FVector(0.5));
        const FVector Center = Placement.WorldCenter(Box);
        return !GetWorld()->OverlapBlockingTestByChannel(Center, Placement.Rotation, ECC_Pawn, Shape, Query)
            && (!bPlanning || !GetWorld()->OverlapBlockingTestByChannel(
                Center, Placement.Rotation, ECC_GameTraceChannel1, Shape, Query));
    };
    for (const FBox& Box : Placement.GetPiece().Boxes)
    {
        if (!IsClear(Box))
        {
            return false;
        }
    }
    return true;
}

TOptional<FSurvivalBuildPlacement> ASurvivalCharacter::FindPlacement() const
{
    FHitResult Hit;
    const bool bPlanHit = FindPlanAimHit(BuildReach, Hit);
    if (!bPlanHit && !FindWorldAimHit(BuildReach, Hit))
    {
        return {};
    }

    const FSurvivalBuildPlacement* Source = nullptr;
    if (ASurvivalBuildChunk* Chunk = Cast<ASurvivalBuildChunk>(Hit.GetActor()))
    {
        if (bPlanHit)
        {
            Source = Chunk->GetPlanTarget(Hit).GetPlan();
        }
        else if (const FSurvivalFinishedPiece* Finished = Chunk->FindFinished(Hit))
        {
            Source = &Finished->Placement;
        }
    }

    FSurvivalBuildPlacement Placement;
    Placement.Base = FVector(FMath::GridSnap(Hit.ImpactPoint.X, FSurvivalBuildPlacement::GridSize),
        FMath::GridSnap(Hit.ImpactPoint.Y, FSurvivalBuildPlacement::GridSize), Hit.ImpactPoint.Z);
    Placement.Rotation = FRotator(0, QuarterTurns * 90.0, 0).Quaternion();
    Placement.PieceIndex = GetSelectedPiece();
    FVector ReachPoint = Hit.ImpactPoint;
    if (Source)
    {
        const FVector TowardViewer = GetAimCamera().GetComponentLocation() - Hit.ImpactPoint;
        if (!Placement.SnapTo(*Source, Hit.ImpactPoint, Hit.ImpactNormal, TowardViewer))
        {
            return {};
        }
    }
    else
    {
        if (Hit.ImpactNormal.Z < 0.95)
        {
            return {};
        }
        ReachPoint = Placement.Base + FVector(0, 0, SupportTolerance + 2.0);
    }

    Placement.AlignFrameEdge(Hit.ImpactPoint);
    const bool bPlannedSupport = bPlanHit && Source && Placement.SupportCostFrom(*Source) >= 0;
    Placement.bClear = HasReachTo(ReachPoint, BuildReach) && IsFootprintClear(Placement, true)
        && (bPlannedSupport || HasSupport(Placement));
    return TOptional<FSurvivalBuildPlacement>(Placement);
}

bool ASurvivalCharacter::CanFinish(const FSurvivalBuildPlacement& Placement) const
{
    return HasSupport(Placement) && IsFootprintClear(Placement, false);
}

void ASurvivalCharacter::PlacePlan()
{
    if (bBuildMode)
    {
        const TOptional<FSurvivalBuildPlacement> Placement = FindPlacement();
        ASurvivalGameMode* GameMode = GetWorld()->GetAuthGameMode<ASurvivalGameMode>();
        if (Placement.IsSet() && Placement->bClear && GameMode)
        {
            GameMode->AddPlan(*Placement);
        }
    }
}

void ASurvivalCharacter::RemovePlan()
{
    if (HealthPoints <= 0 || bInventoryOpen)
    {
        return;
    }
    if (ASurvivalGameMode* GameMode = GetWorld()->GetAuthGameMode<ASurvivalGameMode>())
    {
        GameMode->RemovePlan(FindPlan());
    }
}

void ASurvivalCharacter::Dismantle()
{
    FHitResult Hit;
    if (!bBuildMode || HealthPoints <= 0 || !FindWorldAimHit(InteractReach, Hit)
        || !HasReachTo(Hit.ImpactPoint, InteractReach))
    {
        return;
    }
    if (ASurvivalBuildChunk* Chunk = Cast<ASurvivalBuildChunk>(Hit.GetActor()))
    {
        if (const FSurvivalFinishedPiece* Piece = Chunk->FindFinished(Hit))
        {
            Chunk->DamageFinished(Hit, Piece->Health);
        }
    }
}
