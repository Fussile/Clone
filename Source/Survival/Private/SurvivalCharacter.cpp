#include "SurvivalCharacter.h"

#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RotationMatrix.h"
#include "Sound/SoundBase.h"
#include "SurvivalBuildChunk.h"
#include "SurvivalGameMode.h"
#include "SurvivalHUD.h"
#include "SurvivalRound.h"
#include "SurvivalSidearm.h"
#include "SurvivalSupplyCrate.h"
#include "SurvivalWorldDoor.h"
#include "SurvivalZombie.h"

namespace
{
constexpr double InteractReach = 260.0;
constexpr double BuildReach = 500.0;
constexpr double SupportTolerance = 8.0;
constexpr float WalkSpeed = 600.0f;
constexpr float SprintSpeed = 800.0f;
constexpr float DevWalkSpeed = 10000.0f;
constexpr float CrouchSpeed = 280.0f;
constexpr float ProneSpeed = 120.0f;
constexpr float CrouchHalfHeight = 56.0f;
constexpr float ProneHalfHeight = 34.0f;
constexpr int32 SidearmCapacity = 12;
constexpr double SidearmEyeRelief = 47.0;
constexpr int32 RifleCapacity = 4;
const FName SniperMuzzle(TEXT("ViewMuzzle"));
const FName ViewmodelGripBone(TEXT("PBody_058"));
}

ASurvivalCharacter::ASurvivalCharacter()
{
    Pack.SetNum(PackSize);
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

    ScopeCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("ScopeCapture"));
    ScopeCapture->SetupAttachment(FirstPersonCamera);
    ScopeCapture->SetAbsolute(false, true, false);
    ScopeCapture->bCaptureEveryFrame = false;
    ScopeCapture->bCaptureOnMovement = false;
    ScopeCapture->bAlwaysPersistRenderingState = true;
    ScopeCapture->CaptureSource = SCS_FinalColorLDR;
    ScopeCapture->FOVAngle = 20.0f;

    RifleViewmodel = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("RifleViewmodel"));
    RifleViewmodel->SetupAttachment(FirstPersonCamera);
    RifleViewmodel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RifleViewmodel->SetCastShadow(false);
    RifleViewmodel->SetOnlyOwnerSee(true);
    RifleViewmodel->SetHiddenInGame(true);
    RifleViewmodel->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

    RifleWorldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RifleWorldMesh"));
    RifleWorldMesh->SetupAttachment(GetMesh(), FName(TEXT("RightHand")));
    RifleWorldMesh->SetRelativeRotation(FRotator(0, 0, 90));
    RifleWorldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RifleWorldMesh->SetHiddenInGame(true);

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
    UpdateVitals(DeltaSeconds);
    UpdateRifle();
    UpdateSurvivorAnimation();
    if (bHasSidearm && HealthPoints > 0)
    {
        UpdateSidearmAnimation();
    }
    UpdateWeaponAim(DeltaSeconds);
    SyncPack();
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
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
    PlayerInputComponent->BindAction(TEXT("ToggleDevSpeed"), IE_Pressed, this, &ASurvivalCharacter::ToggleDevSpeed);
#endif
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
    PlayerInputComponent->BindAction(TEXT("AimWeapon"), IE_Pressed, this, &ASurvivalCharacter::PressAim);
    PlayerInputComponent->BindAction(TEXT("AimWeapon"), IE_Released, this, &ASurvivalCharacter::ReleaseAim);
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

void ASurvivalCharacter::ToggleDevSpeed()
{
    if (HealthPoints <= 0 || bInventoryOpen)
    {
        return;
    }

    UCharacterMovementComponent* Movement = GetCharacterMovement();
    if (!bDevSpeedActive)
    {
        NormalAcceleration = Movement->MaxAcceleration;
        Movement->MaxAcceleration = DevWalkSpeed * 2.0f;
    }
    else
    {
        Movement->MaxAcceleration = NormalAcceleration;
        const FVector Horizontal = FVector(Movement->Velocity.X, Movement->Velocity.Y, 0.0)
            .GetClampedToMaxSize(WalkSpeed);
        Movement->Velocity.X = Horizontal.X;
        Movement->Velocity.Y = Horizontal.Y;
    }
    bDevSpeedActive = !bDevSpeedActive;
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

ASurvivalWorldDoor* ASurvivalCharacter::FindWorldDoor(FVector* ImpactPoint) const
{
    FHitResult Hit;
    if (!FindWorldAimHit(InteractReach, Hit) || !HasReachTo(Hit.ImpactPoint, InteractReach))
    {
        return nullptr;
    }
    if (ImpactPoint)
    {
        *ImpactPoint = Hit.ImpactPoint;
    }
    return Cast<ASurvivalWorldDoor>(Hit.GetActor());
}

void ASurvivalCharacter::ServerToggleWorldDoor_Implementation(ASurvivalWorldDoor* Door,
    FVector_NetQuantize10 ImpactPoint)
{
    if (HealthPoints <= 0 || bInventoryOpen || !IsValid(Door) || ImpactPoint.ContainsNaN())
    {
        return;
    }
    const FVector Eyes = FirstPersonCamera->GetComponentLocation();
    const FVector ToDoor = ImpactPoint - Eyes;
    FHitResult Hit;
    // Recheck reach and occlusion using the authority's position. Remote cameras are client-driven.
    if (ToDoor.SizeSquared() > FMath::Square(InteractReach + 1.0)
        || !GetWorld()->LineTraceSingleByChannel(Hit, Eyes, ImpactPoint + 3.0 * ToDoor.GetSafeNormal(),
            ECC_Visibility, FCollisionQueryParams(SCENE_QUERY_STAT(WorldDoorReach), false, this))
        || Hit.GetActor() != Door)
    {
        return;
    }
    if (!Door->TryToggle(GetActorLocation()))
    {
        if (APlayerController* Player = Cast<APlayerController>(GetController()))
        {
            Player->ClientMessage(TEXT("Doorway is blocked."));
        }
    }
}

ASurvivalSupplyCrate* ASurvivalCharacter::FindSupplyCrate(FVector* ImpactPoint) const
{
    FHitResult Hit;
    if (FindWorldAimHit(InteractReach, Hit) && HasReachTo(Hit.ImpactPoint, InteractReach))
    {
        if (ImpactPoint)
        {
            *ImpactPoint = Hit.ImpactPoint;
        }
        return Cast<ASurvivalSupplyCrate>(Hit.GetActor());
    }
    return nullptr;
}

void ASurvivalCharacter::ServerScavenge_Implementation(ASurvivalSupplyCrate* Crate,
    FVector_NetQuantize10 ImpactPoint)
{
    if (HealthPoints <= 0 || !IsValid(Crate) || ImpactPoint.ContainsNaN())
    {
        return;
    }

    const FVector Eyes = FirstPersonCamera->GetComponentLocation();
    const FVector ToCrate = ImpactPoint - Eyes;
    FHitResult Hit;
    if (ToCrate.SizeSquared() > FMath::Square(InteractReach + 1.0)
        || !GetWorld()->LineTraceSingleByChannel(Hit, Eyes, ImpactPoint + 3.0 * ToCrate.GetSafeNormal(),
            ECC_Visibility, FCollisionQueryParams(SCENE_QUERY_STAT(ScavengeReach), false, this))
        || Hit.GetActor() != Crate)
    {
        return;
    }

    const FSurvivalLoot Loot = Crate->TakeLoot();
    if (!Loot.IsEmpty())
    {
        ClientReceiveLoot(Loot);
    }
}

void ASurvivalCharacter::ClientReceiveLoot_Implementation(const FSurvivalLoot& Loot)
{
    FSurvivalMaterials Materials = Loot.Materials;
    constexpr int32 AppraisalBonus[] = {0, 1, 2, 4};
    const int32 ScavengingBonus = (Progression.Rank(ETraining::Scavenging) - 1) / 25
        + AppraisalBonus[Progression.Rank(ETraining::LootAppraisal)];
    Materials.Timber += FMath::Min(ScavengingBonus, Materials.Timber);
    Materials.Stone += FMath::Min(ScavengingBonus, Materials.Stone);
    Inventory.Add(Materials);
    Progression.AwardPractice(ETraining::Scavenging, 35);
    Progression.AwardXP(8);
    Items.Add(Loot.Items);
    if (Loot.bRifle && !bHasRifle)
    {
        bHasRifle = true;
        RifleAmmo = 1;
        LoadRifleViewmodel();
        UpdateRifleView();
    }
    if (Loot.bSidearm && !bHasSidearm)
    {
        EquipSidearm();
    }
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

void ASurvivalCharacter::ServerUseRoofLadder_Implementation(AActor* Ladder, FVector_NetQuantize10 ImpactPoint)
{
    if (!IsValid(Ladder) || !Ladder->ActorHasTag(TEXT("CrossroadsRoofLadder")) || HealthPoints <= 0 || bInventoryOpen)
    {
        return;
    }
    const FVector Eyes = FirstPersonCamera->GetComponentLocation();
    const FVector Ray = ImpactPoint - Eyes;
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(RoofLadder), false, this);
    if (ImpactPoint.ContainsNaN() || Ray.Size() > InteractReach + 1 ||
        !GetWorld()->LineTraceSingleByChannel(Hit, Eyes, ImpactPoint + Ray.GetSafeNormal()*3, ECC_Visibility, Query)
        || Hit.GetActor() != Ladder)
    {
        return;
    }
    FVector Center, Extent;
    Ladder->GetActorBounds(false, Center, Extent);
    const double Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const double Roof = Center.Z + Extent.Z - 100;
    const bool bDescending = GetActorLocation().Z - Half > Roof - 35;
    FVector Bottom(Center.X, Center.Y, Center.Z - Extent.Z + Half + 3);
    FVector Top(Center.X, Center.Y, Roof + Half + 3);
    const FVector Landing = Top + Ladder->GetActorForwardVector()*160;
    const FVector Path[] = {GetActorLocation(), bDescending ? Top : Bottom,
        bDescending ? Bottom : Top, bDescending ? Bottom : Landing};
    Query.AddIgnoredActor(Ladder);
    const FCollisionShape Capsule = FCollisionShape::MakeCapsule(GetCapsuleComponent()->GetScaledCapsuleRadius(), Half);
    for (int32 Step = 1; Step < UE_ARRAY_COUNT(Path); ++Step)
    {
        if (GetWorld()->SweepTestByProfile(Path[Step-1], Path[Step], FQuat::Identity,
                GetCapsuleComponent()->GetCollisionProfileName(), Capsule, Query))
        {
            return;
        }
    }
    GetCharacterMovement()->StopMovementImmediately();
    SetActorLocation(Path[3], false, nullptr, ETeleportType::TeleportPhysics);
    ForceNetUpdate();
}

void ASurvivalCharacter::Interact()
{
    if (HealthPoints <= 0 || bInventoryOpen)
    {
        return;
    }
    FHitResult LadderHit;
    if (FindWorldAimHit(InteractReach, LadderHit) && IsValid(LadderHit.GetActor()) &&
        LadderHit.GetActor()->ActorHasTag(TEXT("CrossroadsRoofLadder")))
    {
        ServerUseRoofLadder(LadderHit.GetActor(), FVector_NetQuantize10(LadderHit.ImpactPoint));
        return;
    }
    FVector InteractionPoint;
    if (ASurvivalWorldDoor* Door = FindWorldDoor(&InteractionPoint))
    {
        ServerToggleWorldDoor(Door, FVector_NetQuantize10(InteractionPoint));
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
                Progression.AwardPractice(ETraining::Construction, 12);
                Progression.AwardXP(12);
            }
        }
    }
    else if (ASurvivalSupplyCrate* Crate = FindSupplyCrate(&InteractionPoint))
    {
        ServerScavenge(Crate, FVector_NetQuantize10(InteractionPoint));
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
                        if (Chunk->TryToggleDoor(Hit, GetActorLocation()))
                        {
                            return;
                        }
                    }
                    else if (Items.TimberDoors > 0 && Chunk->TryInstallDoor(Hit))
                    {
                        --Items.TimberDoors;
                        Progression.AwardPractice(ETraining::Construction, 12);
                        Progression.AwardXP(12);
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
                    const int32 RepairHealth = FMath::RoundToInt(30.0f * Progression.Effect(ETraining::Construction)
                        * Progression.Effect(ETraining::Repair) * Progression.Effect(ETraining::BasicCraft));
                    if (Chunk->RepairFinished(Hit, Inventory, RepairHealth))
                    {
                        Progression.AwardPractice(ETraining::Construction, 8);
                        Progression.AwardPractice(ETraining::Repair, 12);
                        Progression.AwardXP(8);
                    }
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
    if (ScopeTexture && Sidearm)
    {
        ScopeCapture->HideActorComponents(Sidearm.Get(), false);
    }
    LoadSidearmViewmodel();
    UpdateSidearmView();
}

void ASurvivalCharacter::UpdateSidearmView()
{
    UpdateRifleView();
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
            Sidearm->SetActorRelativeLocation(FVector(48, 19, -44));
        }
    }
}

void ASurvivalCharacter::LoadSidearmViewmodel()
{
    constexpr TCHAR MeshPath[] = TEXT("/Game/Art/Crossroads/AlignedSidearm/SK_SidearmViewmodel.SK_SidearmViewmodel");
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
    SidearmHipRotation = CameraAlignment;
    SidearmViewmodel->SetRelativeRotation(CameraAlignment);
    const FVector CameraOffset = CameraAlignment.RotateVector(Eye.GetLocation() * Scale);
    const FVector GripFromEye = CameraAlignment.RotateVector(Reach * Scale);
    ViewmodelHip = FVector(10, 0, -44) - CameraOffset;
    SidearmAimRotation = SidearmHipRotation;
    ViewmodelAim = FVector(10, -GripFromEye.Y, -GripFromEye.Z - 11) - CameraOffset;
    UpdateSidearmSightAlignment();
    SidearmViewmodel->SetRelativeLocation(ViewmodelHip);

    const auto LoadClip = [](const TCHAR* Name)
    {
        const FString Path = FString::Printf(TEXT("/Game/Art/Crossroads/AlignedSidearm/%s.%s"), Name, Name);
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

UTextureRenderTarget2D* ASurvivalCharacter::GetScopeTexture() const
{
    return ScopeTexture.Get();
}

bool ASurvivalCharacter::IsAimingRifle() const
{
    return bAimHeld && RifleHold && bHasRifle && IsRifleSelected() && !bThirdPerson
        && !bBuildMode && !bInventoryOpen && !IsRifleReloading() && HealthPoints > 0;
}

void ASurvivalCharacter::UpdateSidearmSightAlignment()
{
    const FName FrontName(TEXT("FrontSight"));
    const FName RearName(TEXT("RearSight"));
    const FName UpName(TEXT("SightUp"));
    if (!SidearmViewmodel->DoesSocketExist(FrontName) || !SidearmViewmodel->DoesSocketExist(RearName)
        || !SidearmViewmodel->DoesSocketExist(UpName))
    {
        return;
    }
    const FVector Front = SidearmViewmodel->GetSocketTransform(FrontName, RTS_Component).GetLocation();
    const FVector Rear = SidearmViewmodel->GetSocketTransform(RearName, RTS_Component).GetLocation();
    const FVector SightLine = (Front - Rear).GetSafeNormal();
    const FVector SightUp = (SidearmViewmodel->GetSocketTransform(UpName, RTS_Component).GetLocation() - Rear).GetSafeNormal();
    if (!FVector::CrossProduct(SightLine, SightUp).IsNearlyZero())
    {
        // Align the sight line and remove the roll authored into the hip hold.
        SidearmAimRotation = FRotationMatrix::MakeFromXZ(SightLine, SightUp).ToQuat().Inverse();
        ViewmodelAim = FVector(SidearmEyeRelief, 0, 0)
            - SidearmAimRotation.RotateVector(Rear * SidearmViewmodel->GetRelativeScale3D().X);
    }
}

void ASurvivalCharacter::UpdateWeaponAim(float DeltaSeconds)
{
    const bool bHipShotPlaying = ViewmodelFire && CurrentViewmodelAnimation == ViewmodelFire;
    const bool bPistolAiming = IsAimingSidearm() && !bHipShotPlaying;
    const bool bScopeAiming = IsAimingRifle() && IsLocallyControlled() && GetWorld()->GetTimeSeconds() >= RifleReadyAt;
    const float PreviousAim = AimBlend;
    AimBlend = FMath::FInterpConstantTo(AimBlend, bPistolAiming ? 1.0f : 0.0f, DeltaSeconds, 7.0f);
    ScopeBlend = FMath::FInterpConstantTo(ScopeBlend, bScopeAiming ? 1.0f : 0.0f, DeltaSeconds, 6.0f);

    const float PistolAlpha = FMath::SmoothStep(0.0f, 1.0f, AimBlend);
    const float RifleAlpha = FMath::SmoothStep(0.0f, 1.0f, ScopeBlend);
    FirstPersonCamera->SetFieldOfView(90.0f - 16.0f * PistolAlpha - 6.0f * RifleAlpha);
    if (bViewmodelReady && (AimBlend > 0.0f || PreviousAim > 0.0f))
    {
        // Track the idle pose. Leave the last alignment in place during recoil.
        if (CurrentViewmodelAnimation == ViewmodelIdle)
        {
            UpdateSidearmSightAlignment();
        }
        const double ShotAge = GetWorld()->GetTimeSeconds() - SidearmAimedShotAt;
        const double Kick = FMath::Square(FMath::Clamp(1.0 - ShotAge / .14, 0.0, 1.0)) * PistolAlpha;
        const FQuat Recoil = FRotator(2.4 * Kick, 0, 0).Quaternion();
        const FVector RearPivot(SidearmEyeRelief, 0, 0);
        const FVector Pose = FMath::Lerp(ViewmodelHip, ViewmodelAim, PistolAlpha);
        SidearmViewmodel->SetRelativeRotation(Recoil * FQuat::Slerp(SidearmHipRotation, SidearmAimRotation, PistolAlpha));
        SidearmViewmodel->SetRelativeLocation(RearPivot + Recoil.RotateVector(Pose - RearPivot) - FVector(2.0 * Kick, 0, 0));
    }
    else if (!bViewmodelReady && Sidearm && !bThirdPerson && (AimBlend > 0.0f || PreviousAim > 0.0f))
    {
        Sidearm->SetActorRelativeLocation(FMath::Lerp(FVector(48, 19, -44), FVector(48, 0, -7.5), PistolAlpha));
    }

    if (RifleHold)
    {
        RifleViewmodel->SetRelativeLocation(FMath::Lerp(RifleHip, RifleAim, RifleAlpha));
    }
    if (!bThirdPerson && !bInventoryOpen && IsRifleSelected() && (bScopeAiming || ScopeBlend > 0.0f))
    {
        if (!ScopeTexture)
        {
            ScopeTexture = NewObject<UTextureRenderTarget2D>(this);
            ScopeTexture->ClearColor = FLinearColor::Black;
            ScopeTexture->InitCustomFormat(768, 768, PF_B8G8R8A8, false);
            ScopeCapture->TextureTarget = ScopeTexture;
            ScopeCapture->HideActorComponents(this, true);
            if (Sidearm)
            {
                ScopeCapture->HideActorComponents(Sidearm.Get(), false);
            }
        }
        ScopeCapture->SetWorldRotation(GetControlRotation());
        ScopeCapture->CaptureSceneDeferred();
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
            FMath::Lerp(FVector(52, 9, -31), FVector(52, 0, -4), AimBlend));
    if (!FireRound(Muzzle, 5500.0, false))
    {
        return;
    }

    --Ammo;
    NextShotAt = Now + (ViewmodelFire ? ViewmodelFire->GetPlayLength() : 0.23);
    const bool bAimedShot = bViewmodelReady && IsAimingSidearm();
    if (bAimedShot)
    {
        PlaySidearmAnimation(ViewmodelIdle.Get(), true);
        SidearmAimedShotAt = Now;
    }
    else
    {
        PlaySidearmAnimation(ViewmodelFire.Get(), false);
    }
    if (Sidearm)
    {
        Sidearm->PlayShotSound();
    }
    AddControllerPitchInput(bAimedShot ? -0.38f : -0.65f);
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
        Round->SetRifleRound(RifleAssembly.GetDamage());
    }
    return Round != nullptr;

}

void ASurvivalCharacter::LoadRifleViewmodel()
{
    RifleWorldMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,
        TEXT("/Game/Art/Crossroads/SniperSurface/SM_CrossroadsSniper.SM_CrossroadsSniper")));
    USkeletalMesh* Arms = LoadObject<USkeletalMesh>(nullptr,
        TEXT("/Game/Art/Crossroads/SniperSurface/SK_SniperViewmodel.SK_SniperViewmodel"));
    if (!Arms)
    {
        return;
    }
    RifleViewmodel->SetSkeletalMesh(Arms);
    RifleViewmodel->RefreshBoneTransforms(nullptr);
    const FName Breech(TEXT("ViewBreech"));
    if (!RifleViewmodel->DoesSocketExist(SniperMuzzle) || !RifleViewmodel->DoesSocketExist(Breech))
    {
        return;
    }

    const FVector Rear = RifleViewmodel->GetSocketTransform(Breech, RTS_Component).GetLocation();
    const FVector Front = RifleViewmodel->GetSocketTransform(SniperMuzzle, RTS_Component).GetLocation();
    const FVector Barrel = Front - Rear;
    if (Barrel.Size2D() < UE_KINDA_SMALL_NUMBER)
    {
        return;
    }
    const double Scale = 88.6 / Barrel.Size();
    const FRotator Alignment(0, -Barrel.Rotation().Yaw, 0);
    RifleViewmodel->SetRelativeRotation(Alignment);
    RifleViewmodel->SetRelativeScale3D(FVector(Scale));
    // Keep the breech behind the camera and frame the grip and forward section.
    RifleHip = FVector(-4, 3, -24) - Alignment.RotateVector(Rear * Scale);
    // The magnified view is screen-centered independently of the rifle mesh.
    // Draw the rifle into the shoulder with a small lift so the stock stays below the lens.
    RifleAim = RifleHip + FVector(-22, -3, 4);
    RifleViewmodel->SetRelativeLocation(RifleHip);

    const auto LoadClip = [](const TCHAR* Name)
    {
        const FString Path = FString::Printf(TEXT("/Game/Art/Crossroads/SniperSurface/%s.%s"), Name, Name);
        return LoadObject<UAnimSequence>(nullptr, *Path);
    };
    RifleHold = LoadClip(TEXT("SniperHold"));
    RifleFire = LoadClip(TEXT("SniperFire"));
    RifleReload = LoadClip(TEXT("SniperReload"));
}

void ASurvivalCharacter::UpdateRifleView()
{
    const bool bVisible = bHasRifle && IsRifleSelected() && !bBuildMode && !bInventoryOpen && HealthPoints > 0;
    RifleViewmodel->SetHiddenInGame(!bVisible || bThirdPerson || !RifleHold);
    RifleWorldMesh->SetHiddenInGame(!bVisible || !bThirdPerson ||
        !GetMesh()->DoesSocketExist(FName(TEXT("RightHand"))));
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
    CurrentRifleAnimation = nullptr;
    RifleReadyAt = Now + (RifleFire ? RifleFire->GetPlayLength() : 2.0);
    AddControllerPitchInput(-1.35f);
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
        CurrentRifleAnimation = nullptr;
        RifleReloadAt = Now + (RifleReload ? RifleReload->GetPlayLength() : 1.8);
    }
}

void ASurvivalCharacter::UpdateRifle()
{
    const double Now = GetWorld()->GetTimeSeconds();
    if (IsRifleReloading() && Now >= RifleReloadAt)
    {
        const int32 Transfer = FMath::Min(RifleCapacity - RifleAmmo, Items.RifleRounds);
        Items.RifleRounds -= Transfer;
        RifleAmmo += Transfer;
        RifleReloadAt = -1.0;
    }
    UAnimSequence* Desired = RifleHold.Get();
    if (IsRifleReloading() && RifleReload)
    {
        Desired = RifleReload.Get();
    }
    else if (Now < RifleReadyAt && RifleFire)
    {
        Desired = RifleFire.Get();
    }
    if (Desired && Desired != CurrentRifleAnimation)
    {
        CurrentRifleAnimation = Desired;
        RifleViewmodel->PlayAnimation(Desired, Desired == RifleHold.Get());
    }
}

void ASurvivalCharacter::BuyTraining(ETraining Entry, int32 TargetRank)
{
    if (bInventoryOpen && HealthPoints > 0)
    {
        Progression.BuyThrough(Entry, TargetRank);
    }
}

void ASurvivalCharacter::NotifyRoundHit(bool bHitZombie, bool bRifleRound)
{
    if (bHitZombie && HealthPoints > 0)
    {
        Progression.AwardPractice(bRifleRound ? ETraining::Rifles : ETraining::Pistols, 10);
        Progression.AwardPractice(bRifleRound ? ETraining::RifleTechnique : ETraining::PistolTechnique, 10);
        Progression.AwardXP(10);
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
    FVector SpawnPoints[] = {
        FVector(3700.0, 1650.0, 112.0),
        FVector(3490.0, 1410.0, 112.0),
        FVector(3920.0, 1240.0, 112.0)
    };
    static_assert(UE_ARRAY_COUNT(SpawnPoints) == DefenseWaveCount);
    const FString MapName = GetWorld()->GetMapName();
    if (MapName.EndsWith(TEXT("L_Crossroads")) || MapName.EndsWith(TEXT("L_CrossroadsRelief")))
    {
        int32 Found = 0;
        for (TActorIterator<ATargetPoint> Marker(GetWorld()); Marker; ++Marker)
        {
            for (int32 Index = 0; Index < Count; ++Index)
            {
                if (Marker->ActorHasTag(FName(*FString::Printf(TEXT("CrossroadsDefenseSpawn%d"), Index + 1))))
                {
                    SpawnPoints[Index] = Marker->GetActorLocation();
                    ++Found;
                    break;
                }
            }
        }
        // The station encounter is local. Retry if its terrain is out of streaming range.
        if (Found != Count || FVector::DistSquared2D(GetActorLocation(), SpawnPoints[0]) > FMath::Square(5000.0))
        {
            return false;
        }
    }
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
        && !IsAimingSidearm() && !IsAimingRifle() && !bInventoryOpen;
    const bool bSprinting = bSprintAllowed && GetVelocity().SizeSquared2D() > FMath::Square(20.0);
    const ASurvivalGameMode* Mode = GetWorld()->GetAuthGameMode<ASurvivalGameMode>();
    const float Ambient = Mode ? static_cast<float>(Mode->GetAmbientTemperature(GetActorLocation())) : 14.0f;
    const float Effort = static_cast<float>(FMath::Clamp(GetVelocity().Size2D() / WalkSpeed, 0.0, 1.5));
    const int32 Damage = Vitals.Advance(DeltaSeconds, bSprinting, Effort, Ambient,
        Progression.Effect(ETraining::Athletics), Progression.Effect(ETraining::Recovery),
        Progression.Effect(ETraining::Nutrition), Progression.Effect(ETraining::Hydration));
    if (bSprinting)
    {
        constexpr float PracticeInterval = 8.0f;
        SprintPracticeTime += DeltaSeconds;
        const int32 Practices = FMath::FloorToInt(SprintPracticeTime / PracticeInterval);
        SprintPracticeTime -= Practices * PracticeInterval;
        Progression.AwardPractice(ETraining::Athletics, Practices * 10);
        Progression.AwardPractice(ETraining::Recovery, Practices * 10);
        Progression.AwardXP(Practices * 10);
    }
    const float NormalSpeed = bSprintAllowed && Vitals.CanSprint() ? SprintSpeed : WalkSpeed;
    GetCharacterMovement()->MaxWalkSpeed = bDevSpeedActive ? DevWalkSpeed : NormalSpeed;
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
        SyncPack();
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
    if (FSurvivalRifle::PartIndex(Item) != INDEX_NONE)
    {
        // Parts exist only as pack cells, because each carries its own quality.
        int32 Count = 0;
        for (const FSurvivalPackCell& Cell : Pack)
        {
            Count += !Cell.IsEmpty() && Cell.Item == Item ? Cell.Count : 0;
        }
        return Count;
    }
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
    case ESurvivalItem::ScavengerNotes: return Items.ScavengerNotes;
    case ESurvivalItem::DressingSchematic: return Items.DressingSchematics;
    case ESurvivalItem::FieldDressing: return Items.FieldDressings;
    default: return 0;
    }
}

ESurvivalItem ASurvivalCharacter::TrainingBook(ETraining Entry)
{
    switch (Entry)
    {
    case ETraining::Construction: return ESurvivalItem::BuilderGuide;
    case ETraining::LootAppraisal: return ESurvivalItem::ScavengerNotes;
    case ETraining::FieldDressings: return ESurvivalItem::DressingSchematic;
    default: return ESurvivalItem::None;
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
    case ESurvivalItem::Bandage:
    case ESurvivalItem::FieldDressing: return HealthPoints < 100;
    case ESurvivalItem::BuilderGuide: return Progression.CanReadGuide(ETraining::Construction);
    case ESurvivalItem::ScavengerNotes: return Progression.CanReadGuide(ETraining::LootAppraisal);
    case ESurvivalItem::DressingSchematic: return Progression.CanReadGuide(ETraining::FieldDressings);
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
        Progression.AwardPractice(ETraining::Nutrition, FMath::FloorToInt(FMath::Min(34.0f, FSurvivalVitals::Max - Vitals.Hunger)));
        Vitals.Hunger = FMath::Min(FSurvivalVitals::Max, Vitals.Hunger + 34.0f);
        break;
    case ESurvivalItem::Water:
        --Items.Water;
        Progression.AwardPractice(ETraining::Hydration, FMath::FloorToInt(FMath::Min(42.0f, FSurvivalVitals::Max - Vitals.Thirst)));
        Vitals.Thirst = FMath::Min(FSurvivalVitals::Max, Vitals.Thirst + 42.0f);
        break;
    case ESurvivalItem::Bandage:
    case ESurvivalItem::FieldDressing:
    {
        ChangeItemCount(Item, -1);
        const float BaseHeal = Item == ESurvivalItem::Bandage ? 30.0f : 55.0f;
        const int32 Healed = FMath::Min(100 - HealthPoints, FMath::RoundToInt(BaseHeal * Progression.Effect(ETraining::Medicine)));
        HealthPoints += Healed;
        Progression.AwardPractice(ETraining::Medicine, Healed);
        break;
    }
    case ESurvivalItem::BuilderGuide:
        --Items.BuilderGuides;
        Progression.ReadGuide(ETraining::Construction);
        break;
    case ESurvivalItem::ScavengerNotes:
        --Items.ScavengerNotes;
        Progression.ReadGuide(ETraining::LootAppraisal);
        break;
    case ESurvivalItem::DressingSchematic:
        --Items.DressingSchematics;
        Progression.ReadGuide(ETraining::FieldDressings);
        break;
    default:
        break;
    }
}

void ASurvivalCharacter::SetSelectedPackSlot(int32 Slot)
{
    if (Pack.IsValidIndex(Slot) && !Pack[Slot].IsEmpty())
    {
        SelectedPackSlot = Slot;
        SelectedInventoryItem = Pack[Slot].Item;
    }
}

bool ASurvivalCharacter::MoveMenuItem(bool bFromBelt, int32 From, bool bToBelt, int32 To)
{
    if (!bInventoryOpen || HealthPoints <= 0 || From < 0 || From >= (bFromBelt ? HotbarSize : Pack.Num())
        || To < 0 || To >= (bToBelt ? HotbarSize : Pack.Num()))
    {
        return false;
    }
    if (!bFromBelt && !bToBelt)
    {
        return MovePackStack(From, To, false);
    }
    const ESurvivalItem Item = bFromBelt ? Hotbar[From] : GetPackItem(From);
    if (Item == ESurvivalItem::None || SurvivalItemHasQuality(Item) || (!bFromBelt && GetItemCount(Item) <= 0))
    {
        return false;
    }
    if (bFromBelt && bToBelt)
    {
        Swap(Hotbar[From], Hotbar[To]);
    }
    else if (bToBelt)
    {
        SetSelectedPackSlot(From);
        ChooseHotbarSlot(To);
        return true;
    }
    else
    {
        Hotbar[From] = ESurvivalItem::None;
    }
    SelectedInventoryItem = Item;
    UpdateSidearmView();
    return true;
}

bool ASurvivalCharacter::MovePackStack(int32 From, int32 To, bool bHalf)
{
    if (!bInventoryOpen || HealthPoints <= 0 || From == To || !Pack.IsValidIndex(From) || !Pack.IsValidIndex(To)
        || Pack[From].IsEmpty())
    {
        return false;
    }
    FSurvivalPackCell& Source = Pack[From];
    FSurvivalPackCell& Target = Pack[To];
    const int32 Limit = SurvivalItemMaxStack(Source.Item);
    int32 Moved = bHalf ? (Source.Count + 1) / 2 : Source.Count;
    if (Target.IsEmpty())
    {
        Target = Source;
        Target.Count = Moved;
    }
    else if (Target.Item == Source.Item && Limit > 1)
    {
        Moved = FMath::Min(Moved, Limit - Target.Count);
        if (Moved <= 0)
        {
            return false;
        }
        Target.Count += Moved;
    }
    else if (!bHalf)
    {
        Swap(Source, Target);
        SetSelectedPackSlot(To);
        return true;
    }
    else
    {
        return false;
    }
    Source.Count -= Moved;
    if (Source.Count <= 0)
    {
        Source = FSurvivalPackCell();
    }
    SetSelectedPackSlot(To);
    return true;
}

bool ASurvivalCharacter::CanModifyRifle() const
{
    return bInventoryOpen && HealthPoints > 0 && bHasRifle && !IsRifleReloading();
}

bool ASurvivalCharacter::SwapRiflePart(int32 PackSlot)
{
    if (!CanModifyRifle() || !Pack.IsValidIndex(PackSlot) || Pack[PackSlot].IsEmpty())
    {
        return false;
    }
    FSurvivalPackCell& Cell = Pack[PackSlot];
    const int32 Slot = FSurvivalRifle::PartIndex(Cell.Item);
    if (Slot == INDEX_NONE)
    {
        return false;
    }
    // The removed part takes the cell the new part came from.
    Cell.Quality = RifleAssembly.SwapPart(Slot, Cell.Quality);
    SetSelectedPackSlot(PackSlot);
    return true;
}

int32 ASurvivalCharacter::FindFreePackCell()
{
    for (int32 Index = 0; Index < Pack.Num(); ++Index)
    {
        if (Pack[Index].IsEmpty())
        {
            return Index;
        }
    }
    if (Pack.Num() + PackRowWidth <= MaxPackCells)
    {
        const int32 First = Pack.Num();
        Pack.AddDefaulted(PackRowWidth);
        return First;
    }
    return INDEX_NONE;
}

void ASurvivalCharacter::AddToPack(ESurvivalItem Item, int32 Amount, int32 Quality)
{
    const int32 Limit = SurvivalItemMaxStack(Item);
    for (FSurvivalPackCell& Cell : Pack)
    {
        if (Amount > 0 && !Cell.IsEmpty() && Cell.Item == Item && Cell.Count < Limit)
        {
            const int32 Added = FMath::Min(Limit - Cell.Count, Amount);
            Cell.Count += Added;
            Amount -= Added;
        }
    }
    while (Amount > 0)
    {
        const int32 Index = FindFreePackCell();
        if (Index == INDEX_NONE)
        {
            return;
        }
        const int32 Added = FMath::Min(Limit, Amount);
        FSurvivalPackCell& Cell = Pack[Index];
        Cell.Item = Item;
        Cell.Count = Added;
        Cell.Quality = Quality;
        Amount -= Added;
    }
}

void ASurvivalCharacter::RemoveFromPack(ESurvivalItem Item, int32 Amount)
{
    for (int32 Index = Pack.Num() - 1; Index >= 0 && Amount > 0; --Index)
    {
        FSurvivalPackCell& Cell = Pack[Index];
        if (Cell.IsEmpty() || Cell.Item != Item)
        {
            continue;
        }
        const int32 Removed = FMath::Min(Cell.Count, Amount);
        Cell.Count -= Removed;
        Amount -= Removed;
        if (Cell.Count <= 0)
        {
            Cell = FSurvivalPackCell();
        }
    }
}

// Carried totals stay the source of truth for plain items, because building, crafting and ammo all spend them directly.
// The pack only decides where those totals sit: new items top up partial stacks first, spending empties the last stack.
void ASurvivalCharacter::SyncPack()
{
    for (int32 Slot = 0; Slot < FSurvivalRifle::PartCount; ++Slot)
    {
        const ESurvivalItem Part = static_cast<ESurvivalItem>(static_cast<int32>(ESurvivalItem::RifleStock) + Slot);
        while (Items.RifleParts[Slot] > 0)
        {
            const int32 Index = FindFreePackCell();
            if (Index == INDEX_NONE)
            {
                break;
            }
            Pack[Index].Item = Part;
            Pack[Index].Count = 1;
            Pack[Index].Quality = FSurvivalRifle::SpareQuality(Slot);
            --Items.RifleParts[Slot];
        }
    }
    for (int32 Index = 0; Index < static_cast<int32>(ESurvivalItem::None); ++Index)
    {
        const ESurvivalItem Item = static_cast<ESurvivalItem>(Index);
        if (SurvivalItemHasQuality(Item))
        {
            continue;
        }
        int32 InPack = 0;
        for (const FSurvivalPackCell& Cell : Pack)
        {
            InPack += !Cell.IsEmpty() && Cell.Item == Item ? Cell.Count : 0;
        }
        const int32 Total = GetItemCount(Item);
        if (InPack < Total)
        {
            AddToPack(Item, Total - InPack, 0);
        }
        else if (InPack > Total)
        {
            RemoveFromPack(Item, InPack - Total);
        }
    }
    while (Pack.Num() > PackSize)
    {
        bool bEmptyRow = true;
        for (int32 Index = Pack.Num() - PackRowWidth; Index < Pack.Num(); ++Index)
        {
            bEmptyRow = bEmptyRow && Pack[Index].IsEmpty();
        }
        if (!bEmptyRow)
        {
            break;
        }
        Pack.SetNum(Pack.Num() - PackRowWidth);
    }
    if (!Pack.IsValidIndex(SelectedPackSlot) || Pack[SelectedPackSlot].IsEmpty())
    {
        SelectedPackSlot = INDEX_NONE;
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
        if (SelectedInventoryItem != ESurvivalItem::None
            && (GetItemCount(SelectedInventoryItem) <= 0 || SurvivalItemHasQuality(SelectedInventoryItem)))
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
    const FSurvivalRecipe& Recipe = Recipes[RecipeIndex];
    if (Recipe.RequiredTraining != ETraining::Count && Progression.Rank(Recipe.RequiredTraining) == 0)
    {
        return false;
    }
    for (const FSurvivalIngredient& Ingredient : Recipe.Ingredients)
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
    Progression.AwardPractice(ETraining::BasicCraft, 4 * Quantity);
    Progression.AwardXP(4 * Quantity);
}

void ASurvivalCharacter::ChangeItemCount(ESurvivalItem Item, int32 Delta)
{
    switch (Item)
    {
    case ESurvivalItem::Food: Items.Food += Delta; break;
    case ESurvivalItem::Water: Items.Water += Delta; break;
    case ESurvivalItem::Bandage: Items.Bandages += Delta; break;
    case ESurvivalItem::FieldDressing: Items.FieldDressings += Delta; break;
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
