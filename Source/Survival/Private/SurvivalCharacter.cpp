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
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "SurvivalBuildChunk.h"
#include "SurvivalGameMode.h"
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
    Body->SetSkeletalMesh(SurvivorMesh);
    const FBoxSphereBounds Bounds = SurvivorMesh->GetBounds();
    const double Scale = 175.0 / FMath::Max(1.0, 2.0 * Bounds.BoxExtent.Z);
    BodyMeshBottom = (Bounds.Origin.Z - Bounds.BoxExtent.Z) * Scale;
    Body->SetRelativeScale3D(FVector(Scale));
    Body->SetRelativeRotation(FRotator(0, -90, 0));
    UpdateMovementPresentation();

    const auto LoadClip = [](const TCHAR* Name)
    {
        const FString Path = FString::Printf(TEXT("/Game/Art/Crossroads/Player/%s.%s"), Name, Name);
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
    UpdateSurvivorAnimation();
}

void ASurvivalCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateSurvivorAnimation();
    if (bHasSidearm && HealthPoints > 0)
    {
        UpdateSidearmAnimation();
    }
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
    if (Desired && Desired != CurrentAnimation.Get())
    {
        CurrentAnimation = Desired;
        GetMesh()->PlayAnimation(Desired, Desired != JumpAnimation.Get());
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
    PlayerInputComponent->BindAction(TEXT("ToggleBuildMode"), IE_Pressed, this, &ASurvivalCharacter::ToggleBuildMode);
    PlayerInputComponent->BindAction(TEXT("RotatePiece"), IE_Pressed, this, &ASurvivalCharacter::RotatePiece);
    PlayerInputComponent->BindAction(TEXT("NextPiece"), IE_Pressed, this, &ASurvivalCharacter::NextPiece);
    PlayerInputComponent->BindAction(TEXT("PreviousPiece"), IE_Pressed, this, &ASurvivalCharacter::PreviousPiece);
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
    if (!bIsCrouched)
    {
        Jump();
    }
}

void ASurvivalCharacter::StartSprint()
{
    bSprintHeld = true;
    UpdateMovementPresentation();
}

void ASurvivalCharacter::StopSprint()
{
    bSprintHeld = false;
    UpdateMovementPresentation();
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
    if (HealthPoints <= 0 || !Movement->IsMovingOnGround())
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
    Movement->MaxWalkSpeed = bSprintHeld ? SprintSpeed : WalkSpeed;
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
    FirstPersonCamera->SetRelativeLocation(FVector(0, 0, EyeHeight));
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
    if (HealthPoints <= 0)
    {
        return;
    }
    const FSurvivalPlanTarget Target = FindPlan();
    if (const FSurvivalBuildPlacement* Plan = Target.GetPlan())
    {
        if (ASurvivalGameMode* GameMode = GetWorld()->GetAuthGameMode<ASurvivalGameMode>();
            GameMode && CanFinish(*Plan))
        {
            GameMode->FinishPlan(Target, Inventory);
        }
    }
    else if (ASurvivalSupplyCrate* Crate = FindSupplyCrate())
    {
        if (!bHasSidearm && Crate->TakeSidearm())
        {
            EquipSidearm();
        }
        else
        {
            Inventory.Add(Crate->TakeMaterials());
        }
    }
    else
    {
        FHitResult Hit;
        if (FindWorldAimHit(InteractReach, Hit) && HasReachTo(Hit.ImpactPoint, InteractReach))
        {
            if (ASurvivalBuildChunk* Chunk = Cast<ASurvivalBuildChunk>(Hit.GetActor()))
            {
                Chunk->RepairFinished(Hit, Inventory);
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
    Sidearm = GetWorld()->SpawnActor<ASurvivalSidearm>();
    LoadSidearmViewmodel();
    UpdateSidearmView();
}

void ASurvivalCharacter::UpdateSidearmView()
{
    const bool bVisible = !bBuildMode && HealthPoints > 0;
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
        if (bViewmodelReady)
        {
            Sidearm->AttachToComponent(SidearmViewmodel, FAttachmentTransformRules::SnapToTargetNotIncludingScale,
                ViewmodelGripBone);
            Sidearm->SetActorRelativeLocation(ViewmodelGrip.GetLocation());
            Sidearm->SetActorRelativeRotation(ViewmodelGrip.GetRotation());
        }
        else
        {
            Sidearm->AttachToComponent(FirstPersonCamera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
            Sidearm->SetActorRelativeLocation(FVector(48, 19, -28));
        }
        Sidearm->SetActorHiddenInGame(!bVisible);
    }
}

void ASurvivalCharacter::LoadSidearmViewmodel()
{
    constexpr TCHAR MeshPath[] = TEXT("/Game/Art/Crossroads/SidearmViewmodel/SK_SidearmViewmodel.SK_SidearmViewmodel");
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
    SidearmViewmodel->SetRelativeLocation(FVector(10, 0, -26) - CameraAlignment.RotateVector(Eye.GetLocation() * Scale));

    const FVector GripOffset = CameraAlignment.UnrotateVector(FVector(1, 2.5, 8) / Scale);
    ViewmodelGrip = FTransform(Grip.GetRotation().Inverse() * CameraAlignment.Inverse(),
        Grip.GetRotation().UnrotateVector(GripOffset));
    const auto LoadClip = [](const TCHAR* Name)
    {
        const FString Path = FString::Printf(TEXT("/Game/Art/Crossroads/SidearmViewmodel/%s.%s"), Name, Name);
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
        const int32 Transfer = FMath::Min(ReserveAmmo, SidearmCapacity - Ammo);
        Ammo += Transfer;
        ReserveAmmo -= Transfer;
        ReloadCompleteAt = -1.0;
    }
    if (!IsReloading() && Now >= NextShotAt)
    {
        const bool bMoving = GetVelocity().SizeSquared2D() > FMath::Square(20.0);
        PlaySidearmAnimation(bMoving && ViewmodelWalk ? ViewmodelWalk.Get() : ViewmodelIdle.Get(), true);
    }
}

void ASurvivalCharacter::StartReload()
{
    const double Now = GetWorld()->GetTimeSeconds();
    if (HealthPoints <= 0 || !bHasSidearm || IsReloading() || Now < NextShotAt
        || Ammo >= SidearmCapacity || ReserveAmmo <= 0)
    {
        return;
    }
    UAnimSequence* Clip = Ammo == 0 && ViewmodelReloadEmpty ? ViewmodelReloadEmpty.Get() : ViewmodelReload.Get();
    ReloadCompleteAt = Now + (Clip ? Clip->GetPlayLength() : 1.4);
    PlaySidearmAnimation(Clip, false);
}

bool ASurvivalCharacter::ShowHitMarker() const
{
    return GetWorld()->GetTimeSeconds() < HitMarkerUntil;
}

void ASurvivalCharacter::PrimaryAction()
{
    if (HealthPoints <= 0)
    {
        return;
    }
    if (bBuildMode)
    {
        PlacePlan();
        return;
    }
    const double Now = GetWorld()->GetTimeSeconds();
    if (!bHasSidearm || Ammo <= 0 || IsReloading() || Now < NextShotAt)
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
    AddControllerPitchInput(-0.65f);
    constexpr double Range = 4000.0;
    FHitResult Aim;
    if (FindWorldAimHit(Range, Aim))
    {
        const FVector Eyes = FirstPersonCamera->GetComponentLocation();
        const FVector End = Aim.ImpactPoint + (Aim.ImpactPoint - Eyes).GetSafeNormal() * 3.0;
        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByChannel(Hit, Eyes, End, ECC_Visibility,
            FCollisionQueryParams(SCENE_QUERY_STAT(Sidearm), false, this)))
        {
            if (ASurvivalZombie* Zombie = Cast<ASurvivalZombie>(Hit.GetActor()))
            {
                Zombie->HitBySidearm();
                HitMarkerUntil = GetWorld()->GetTimeSeconds() + 0.15;
            }
        }
    }
}

int32 ASurvivalCharacter::GetLivingAttackers() const
{
    int32 Alive = 0;
    for (const TWeakObjectPtr<ASurvivalZombie>& Attacker : Attackers)
    {
        if (Attacker.IsValid())
        {
            ++Alive;
        }
    }
    return Alive;
}

void ASurvivalCharacter::StartDefense()
{
    if (HealthPoints <= 0 || DefenseWave >= DefenseWaveCount || GetLivingAttackers() > 0)
    {
        return;
    }

    const FVector SpawnPoints[] = {
        FVector(3700.0, 1650.0, 112.0),
        FVector(3490.0, 1410.0, 112.0),
        FVector(3920.0, 1240.0, 112.0)
    };
    static_assert(UE_ARRAY_COUNT(SpawnPoints) == DefenseWaveCount);
    const int32 NextWave = DefenseWave + 1;
    Attackers.Reset();
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
    for (int32 Index = 0; Index < NextWave; ++Index)
    {
        ASurvivalZombie* Zombie = GetWorld()->SpawnActor<ASurvivalZombie>(
            SpawnPoints[Index], FRotator(0, 90, 0), SpawnParameters);
        if (!Zombie)
        {
            for (const TWeakObjectPtr<ASurvivalZombie>& Spawned : Attackers)
            {
                if (ASurvivalZombie* PartialWave = Spawned.Get())
                {
                    PartialWave->Destroy();
                }
            }
            Attackers.Reset();
            return;
        }
        Attackers.Add(Zombie);
    }

    DefenseWave = NextWave;
    if (DefenseWave > 1 && bHasSidearm)
    {
        ReserveAmmo += 6;
    }
}

void ASurvivalCharacter::ReceiveZombieHit(int32 Amount)
{
    HealthPoints = FMath::Max(0, HealthPoints - Amount);
    if (HealthPoints == 0)
    {
        bBuildMode = false;
        ReloadCompleteAt = -1.0;
        UpdateSidearmView();
        GetCharacterMovement()->DisableMovement();
    }
}

void ASurvivalCharacter::RestartDefense()
{
    UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
}

void ASurvivalCharacter::ToggleBuildMode()
{
    bBuildMode = HealthPoints > 0 && !bBuildMode;
    UpdateSidearmView();
}

void ASurvivalCharacter::RotatePiece()
{
    if (bBuildMode)
    {
        QuarterTurns = (QuarterTurns + 1) % 4;
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
        SelectedPiece = (SelectedPiece + 1) % GetSurvivalBuildPieces().Num();
    }
}

void ASurvivalCharacter::PreviousPiece()
{
    if (bBuildMode)
    {
        const int32 Count = GetSurvivalBuildPieces().Num();
        SelectedPiece = (SelectedPiece + Count - 1) % Count;
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

    for (const FBox& Box : Placement.GetPiece().Boxes)
    {
        // A small inset lets adjoining boxes touch without rejecting the placement.
        const FCollisionShape Shape = FCollisionShape::MakeBox(Box.GetExtent() - FVector(0.5));
        const FVector Center = Placement.WorldCenter(Box);
        if (GetWorld()->OverlapBlockingTestByChannel(Center, Placement.Rotation, ECC_Pawn, Shape, Query)
            || (bPlanning && GetWorld()->OverlapBlockingTestByChannel(
                Center, Placement.Rotation, ECC_GameTraceChannel1, Shape, Query)))
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
    Placement.PieceIndex = SelectedPiece;
    FVector ReachPoint = Hit.ImpactPoint;
    if (Source && Placement.GetPiece().bStairs && Source->IsUpperStairEdge(Hit.ImpactPoint, Hit.ImpactNormal))
    {
        Placement.Base = Source->StairContinuation();
        Placement.Rotation = Source->Rotation;
    }
    else if (Hit.ImpactNormal.Z < 0.95)
    {
        if (!Source || !Placement.GetPiece().bCarriesLateralSupport || FMath::Abs(Hit.ImpactNormal.Z) > 0.1)
        {
            return {};
        }

        Placement.Base = Source->Base;
        if (Source->GetPiece().bCarriesLateralSupport)
        {
            const FVector Normal = Hit.ImpactNormal;
            const FVector Axis = FMath::Abs(Normal.X) > FMath::Abs(Normal.Y)
                ? FVector(FMath::Sign(Normal.X), 0, 0) : FVector(0, FMath::Sign(Normal.Y), 0);
            Placement.Base += Axis * FSurvivalBuildPlacement::GridSize;
        }
        else
        {
            double Top = 0.0;
            for (const FBox& Box : Source->GetPiece().Boxes)
            {
                Top = FMath::Max(Top, Box.Max.Z);
            }
            Placement.Base.Z += Top;
        }
    }
    else if (Source)
    {
        Placement.Base.X = Source->Base.X;
        Placement.Base.Y = Source->Base.Y;
    }
    else
    {
        ReachPoint = Placement.Base + FVector(0, 0, SupportTolerance + 2.0);
    }

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
    if (HealthPoints <= 0)
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
