#include "SurvivalCharacter.h"
#include "Algo/Find.h"
#include "SurvivalGear.h"
#include "SurvivalWeaponMods.h"
#include "SurvivalArsenal.h"
#include "InputCoreTypes.h"
#include "Net/UnrealNetwork.h"

#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
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
#include "SurvivalLootBudget.h"
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
UStaticMesh* FirearmWorldMesh(int32 Model)
{
    if (Model < 2 || Model >= UE_ARRAY_COUNT(Arsenal)) return nullptr;
    const FString Path = FString::Printf(TEXT("/Game/Art/Crossroads/Arsenal/%s/Geometry/World.World"), Arsenal[Model].Asset);
    return LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn);
}
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
    PortraitCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("EquipmentPortrait"));
    PortraitCapture->SetupAttachment(GetRootComponent());
    PortraitCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    PortraitCapture->CaptureSource = SCS_FinalColorLDR;
    PortraitCapture->ShowFlags.SetLighting(false);
    PortraitCapture->bCaptureEveryFrame = false;
    PortraitCapture->bCaptureOnMovement = false;
    InspectionWeapon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InspectionWeapon"));
    InspectionWeapon->SetupAttachment(GetRootComponent());
    InspectionWeapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    InspectionWeapon->SetVisibleInSceneCaptureOnly(true);
    InspectionWeapon->SetVisibility(false);
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
    UpdateSurvivorAnimation(0.0f);
}

void ASurvivalCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (HasAuthority() && HealthPoints > 0)
    {
        SurvivalAgeSeconds += DeltaSeconds;
        const auto* Mode = GetWorld()->GetAuthGameMode<ASurvivalGameMode>();
        const float DayLength = Mode ? Mode->GetDayLengthSeconds() : 3600.0f;
        const int32 DaysSurvived = FMath::FloorToInt32(SurvivalAgeSeconds / DayLength);
        LootGameStage = SurvivalLootBudget::GetGameStage(Progression.CharacterLevel, DaysSurvived);
    }
    if (HasAuthority() && (bSearchingContainer || bViewingContainer))
        UpdateContainerSearch(DeltaSeconds);
    UpdateVitals(DeltaSeconds);
    UpdateRifle();
    UpdateSurvivorAnimation(DeltaSeconds);
    if (bHasSidearm && HealthPoints > 0)
    {
        UpdateSidearmAnimation();
    }
    UpdateWeaponAim(DeltaSeconds);
    SyncPack();
}

void ASurvivalCharacter::UpdateSurvivorAnimation(float DeltaSeconds)
{
    const FVector Velocity = GetVelocity();
    const double SpeedSquared = Velocity.SizeSquared2D();
    const bool bMoving = SpeedSquared > FMath::Square(20.0);
    const UCharacterMovementComponent* Movement = GetCharacterMovement();
    const bool bAirborne = Movement->IsFalling();
    AirborneAnimationTime = bAirborne ? AirborneAnimationTime + DeltaSeconds : 0.0f;
    // Jump immediately on takeoff, but ignore brief floor gaps while running.
    const bool bShowJump = bAirborne && (Velocity.Z > 80.0 || AirborneAnimationTime > 0.10f
        || (JumpAnimation && CurrentAnimation == JumpAnimation));
    UAnimSequence* Desired = IdleAnimation.Get();
    if (Stance == EStance::Prone)
    {
        Desired = bMoving ? ProneMoveAnimation.Get() : ProneIdleAnimation.Get();
    }
    else if (bIsCrouched)
    {
        Desired = bMoving ? CrouchMoveAnimation.Get() : CrouchIdleAnimation.Get();
    }
    else if (bShowJump && JumpAnimation)
    {
        Desired = JumpAnimation.Get();
    }
    else if (bMoving)
    {
        // The local player already knows the sprint cap. Remote movement uses
        // two speed thresholds so floor and replication jitter won't restart the clip.
        const bool bLocalSprint = IsLocallyControlled() && Movement->MaxWalkSpeed >= SprintSpeed;
        const float RunEntrySpeed = bLocalSprint ? WalkSpeed * 0.8f : WalkSpeed + 45.0f;
        const float RunExitSpeed = bLocalSprint ? WalkSpeed * 0.65f : WalkSpeed + 10.0f;
        bPlayingRun = SpeedSquared > FMath::Square(bPlayingRun ? RunExitSpeed : RunEntrySpeed);
        Desired = bPlayingRun && RunAnimation ? RunAnimation.Get() : WalkAnimation.Get();
    }
    else
    {
        bPlayingRun = false;
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
    PlayerInputComponent->BindAction(TEXT("CloseLoot"), IE_Pressed, this, &ASurvivalCharacter::CloseContainer);
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
    for (UStaticMeshComponent* Part : GearMeshes) Part->SetOwnerNoSee(!bThirdPerson);
    UpdateSidearmView();
    RefreshAttachments();
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

bool ASurvivalCharacter::CanReachLoot(const ASurvivalSupplyCrate* Crate, FVector ImpactPoint) const
{
    if (HealthPoints <= 0 || !IsValid(Crate) || ImpactPoint.ContainsNaN()) return false;

    const FVector Eyes = FirstPersonCamera->GetComponentLocation();
    const FVector ToCrate = ImpactPoint - Eyes;
    FHitResult Hit;
    return ToCrate.SizeSquared() <= FMath::Square(InteractReach + 1.0)
        && GetWorld()->LineTraceSingleByChannel(Hit, Eyes, ImpactPoint + 3.0 * ToCrate.GetSafeNormal(),
            ECC_Visibility, FCollisionQueryParams(SCENE_QUERY_STAT(ScavengeReach), false, this))
        && Hit.GetActor() == Crate;
}

void ASurvivalCharacter::ServerScavenge_Implementation(ASurvivalSupplyCrate* Crate, FVector_NetQuantize10 ImpactPoint)
{
    if (!CanReachLoot(Crate, ImpactPoint) || Crate->IsDepleted()) return;

    SearchImpactPoint = ImpactPoint;
    if (Crate->HasBeenSearched())
    {
        SearchProgress.Remove(Crate->GetLootId());
        bSearchingContainer = false;
        ClientSearchTime(0, false);
        ShowContainer(*Crate, true);
        return;
    }

    bViewingContainer = false;
    SearchingContainer = Crate;
    ActiveSearchId = Crate->GetLootId();
    FSearchProgress* Entry = SearchProgress.Find(ActiveSearchId);
    if (!Entry || Entry->Seed != Crate->GetLootSeed())
        Entry = &SearchProgress.Add(ActiveSearchId, FSearchProgress{Crate->GetLootSeed(), Crate->GetSearchDuration()});
    bSearchingContainer = true;
    ClientSearchTime(Entry->SecondsLeft, false);
}

void ASurvivalCharacter::UpdateContainerSearch(float DeltaSeconds)
{
    if (bViewingContainer)
    {
        ASurvivalSupplyCrate* Crate = ServerContainer.Get();
        if (!CanReachLoot(Crate, SearchImpactPoint) || !Crate->HasBeenSearched())
        {
            ServerCloseContainer_Implementation();
        }
        else if (Crate->GetLootRevision() != ServerSeenLootRevision)
            ShowContainer(*Crate, false);
    }

    if (!bSearchingContainer) return;
    ASurvivalSupplyCrate* Crate = SearchingContainer.Get();
    FSearchProgress* Entry = SearchProgress.Find(ActiveSearchId);
    if (!CanReachLoot(Crate, SearchImpactPoint) || !Entry || Entry->Seed != Crate->GetLootSeed())
    {
        bSearchingContainer = false;
        ClientSearchTime(0, false);
        return;
    }

    if (!Crate->HasBeenSearched() && (Entry->SecondsLeft -= DeltaSeconds) > 0) return;

    const bool bFirstSearch = Crate->FinishSearch(LootGameStage);
    if (bFirstSearch)
    {
        Progression.AwardPractice(ETraining::Scavenging, 35);
        Progression.AwardXP(8);
    }
    SearchProgress.Remove(ActiveSearchId);
    bSearchingContainer = false;
    ClientSearchTime(0, bFirstSearch);
    ShowContainer(*Crate, true);
}

void ASurvivalCharacter::ClientSearchTime_Implementation(float Seconds, bool bFinished)
{
    SearchEndsAt = Seconds > 0 ? GetWorld()->GetTimeSeconds() + Seconds : 0;
    if (bFinished && !HasAuthority())
    {
        Progression.AwardPractice(ETraining::Scavenging, 35);
        Progression.AwardXP(8);
    }
}

float ASurvivalCharacter::GetSearchSecondsLeft() const
{
    return FMath::Max(0.0, SearchEndsAt - GetWorld()->GetTimeSeconds());
}

void ASurvivalCharacter::ShowContainer(ASurvivalSupplyCrate& Crate, bool bOpenMenu)
{
    if (Crate.IsDepleted())
    {
        ServerCloseContainer_Implementation();
        return;
    }
    ServerContainer = &Crate;
    bViewingContainer = true;
    ServerSeenLootRevision = Crate.GetLootRevision();
    ClientShowContainer(&Crate, Crate.GetSlots(), ServerSeenLootRevision, bOpenMenu);
}

void ASurvivalCharacter::ClientShowContainer_Implementation(ASurvivalSupplyCrate* Crate,
    const TArray<FSurvivalLootSlot>& Slots, int32 Revision, bool bOpenMenu)
{
    if (Crate && (HealthPoints <= 0 || (!bOpenMenu && LootContainer.Get() != Crate))) return;
    const bool bWasOpen = HasOpenContainer();
    LootContainer = Crate;
    if (Crate) ContainerSlots = Slots;
    else ContainerSlots.Reset();
    ContainerRevision = Crate ? Revision : 0;
    const bool bIsOpen = HasOpenContainer();
    if (bIsOpen) bAimHeld = false;
    if (bWasOpen != bIsOpen)
    {
        if (APlayerController* Player = Cast<APlayerController>(GetController()))
            if (ASurvivalHUD* HUD = Cast<ASurvivalHUD>(Player->GetHUD()))
                HUD->SetLootVisible(*this, bIsOpen);
    }
}

void ASurvivalCharacter::CloseContainer()
{
    if (!HasOpenContainer() && !bViewingContainer) return;
    if (!HasAuthority())
        ClientShowContainer_Implementation(nullptr, TArray<FSurvivalLootSlot>{}, 0, false);
    ServerCloseContainer();
}

void ASurvivalCharacter::ServerCloseContainer_Implementation()
{
    ServerContainer.Reset();
    bViewingContainer = false;
    ClientShowContainer(nullptr, TArray<FSurvivalLootSlot>{}, 0, false);
}

const FSurvivalLootSlot* ASurvivalCharacter::GetContainerSlot(int32 Index) const
{
    return ContainerSlots.IsValidIndex(Index) ? &ContainerSlots[Index] : nullptr;
}

namespace
{
bool PlaceLootStack(TArray<FSurvivalPackCell>& Pack, const FSurvivalLootSlot& Loot, int32 Target, int32 WeaponId = INDEX_NONE)
{
    const int32 Limit = Loot.GearIndex != INDEX_NONE ? MAX_int32 : SurvivalItemMaxStack(Loot.Item());
    FSurvivalPackCell Incoming = Loot.ToPackCell();
    Incoming.WeaponId = WeaponId;
    const auto Matches = [&Loot, Limit](const FSurvivalPackCell& Cell)
    {
        return Limit > 1 && !Cell.IsEmpty() && Cell.Item == Loot.Item() && Cell.GearIndex == Loot.GearIndex;
    };
    if (Target != INDEX_NONE)
    {
        if (!Pack.IsValidIndex(Target)) return false;
        auto& Destination = Pack[Target];
        if (Destination.IsEmpty()) Destination = Incoming;
        else if (Matches(Destination) && Destination.Count <= Limit - Loot.Count) Destination.Count += Loot.Count;
        else return false;
        return true;
    }

    int32 Capacity = 0;
    for (const auto& Cell : Pack)
    {
        if (Cell.IsEmpty()) Capacity += FMath::Min(Limit, Loot.Count - Capacity);
        else if (Matches(Cell)) Capacity += FMath::Clamp(Limit - Cell.Count, 0, Loot.Count - Capacity);
        if (Capacity >= Loot.Count) break;
    }
    if (Capacity < Loot.Count)
    {
        if (Pack.Num() + ASurvivalCharacter::PackRowWidth > ASurvivalCharacter::MaxPackCells) return false;
        Pack.AddDefaulted(ASurvivalCharacter::PackRowWidth);
    }

    int32 Remaining = Loot.Count;
    for (auto& Cell : Pack)
    {
        if (!Matches(Cell)) continue;
        const int32 Moved = FMath::Clamp(Limit - Cell.Count, 0, Remaining);
        Cell.Count += Moved;
        Remaining -= Moved;
        if (Remaining == 0) return true;
    }
    for (auto& Cell : Pack)
    {
        if (!Cell.IsEmpty()) continue;
        Cell = Incoming;
        Cell.Count = FMath::Min(Remaining, Limit);
        Remaining -= Cell.Count;
        if (Remaining == 0) return true;
    }
    return false;
}

bool IsWeaponLoot(ESurvivalItem Item)
{
    return Item == ESurvivalItem::Sidearm || Item == ESurvivalItem::Rifle;
}
}

void ASurvivalCharacter::TakeContainerLoot(int32 Index, int32 PackTarget)
{
    if (!LootContainer.IsValid()) return;
    TArray<FSurvivalPackCell> Preview = Pack;
    TArray<int32> Indices;
    for (int32 Slot = 0; Slot < ContainerSlots.Num(); ++Slot)
    {
        if (Index != INDEX_NONE && Slot != Index) continue;
        const auto& Loot = ContainerSlots[Slot];
        if (Loot.IsEmpty()) continue;
        if (PlaceLootStack(Preview, Loot, PackTarget)) Indices.Add(Slot);
    }
    if (!Indices.IsEmpty())
        ServerTakeContainerLoot(LootContainer.Get(), ContainerRevision, Indices, PackTarget);
}

bool ASurvivalCharacter::ValidateContainer(ASurvivalSupplyCrate* Crate, int32 Revision)
{
    if (!bViewingContainer || ServerContainer.Get() != Crate || !CanReachLoot(Crate, SearchImpactPoint) ||
        !Crate->HasBeenSearched())
        return false;
    if (Crate->GetLootRevision() == Revision) return true;
    ShowContainer(*Crate, false);
    return false;
}

void ASurvivalCharacter::ServerTakeContainerLoot_Implementation(ASurvivalSupplyCrate* Crate,
    int32 Revision, const TArray<int32>& Indices, int32 PackTarget)
{
    if (!ValidateContainer(Crate, Revision) || Indices.Num() > ASurvivalSupplyCrate::MaxLootSlots ||
        (PackTarget != INDEX_NONE && (PackTarget < 0 || PackTarget >= MaxPackCells || Indices.Num() != 1)))
        return;

    const bool bApplyAppraisal = !Crate->IsDroppedLoot();
    const auto Taken = Crate->TakeLoot(Indices);
    for (const auto& Loot : Taken)
        if (Loot.GearIndex != INDEX_NONE) OwnedGear.Add(Loot.GearIndex);
    if (!Taken.IsEmpty()) ClientReceiveLoot(Taken, PackTarget, bApplyAppraisal);
    if (IsValid(Crate)) ShowContainer(*Crate, false);
    else
    {
        ServerCloseContainer_Implementation();
    }
}

void ASurvivalCharacter::MoveContainerLoot(int32 From, int32 To, bool bSplit)
{
    if (LootContainer.IsValid())
        ServerMoveContainerLoot(LootContainer.Get(), ContainerRevision, From, To, bSplit);
}

void ASurvivalCharacter::ServerMoveContainerLoot_Implementation(ASurvivalSupplyCrate* Crate,
    int32 Revision, int32 From, int32 To, bool bSplit)
{
    if (!ValidateContainer(Crate, Revision)) return;
    Crate->MoveLoot(From, To, bSplit);
    ShowContainer(*Crate, false);
}

void ASurvivalCharacter::ClientReceiveLoot_Implementation(const TArray<FSurvivalLootSlot>& Taken,
    int32 PackTarget, bool bApplyAppraisal)
{
    for (const auto& Loot : Taken)
    {
        const auto Item = Loot.Item();

        if (Loot.GearIndex != INDEX_NONE && !HasAuthority())
        {
            if (PackTarget != INDEX_NONE) PendingGearTargets.Add(Loot.GearIndex, PackTarget);
            continue; // The replicated OwnedGear array supplies the actual item.
        }
        int32 WeaponId = INDEX_NONE;
        if (IsWeaponLoot(Item))
        {
            const int32 Quality = Loot.Quality > 0 ? Loot.Quality : FSurvivalWeaponAssembly::FactoryQuality;
            FWeaponInstance Instance{Item, {}, Item == ESurvivalItem::Sidearm ? SidearmCapacity : 1, 0};
            Instance.Firearm = FirearmIndex(Item, Loot.Firearm);
            Instance.Ammo = Item == ESurvivalItem::Sidearm ? Arsenal[Instance.Firearm].Capacity : 1;
            for (int32 Slot = 0; Slot < FSurvivalWeaponAssembly::PartCount; ++Slot)
                Instance.Assembly.SwapPart(Slot, Quality);
            WeaponId = WeaponItems.Add(Instance);

            if (Item == ESurvivalItem::Sidearm && !bHasSidearm)
            {
                EquipSidearm();
            }
            else if (Item == ESurvivalItem::Rifle && !bHasRifle)
            {
                bHasRifle = true;
                LoadRifleViewmodel();
                UpdateRifleView();
            }
        }
        bool bPlaced = PlaceLootStack(Pack, Loot, PackTarget, WeaponId);
        if (!bPlaced && PackTarget != INDEX_NONE) bPlaced = PlaceLootStack(Pack, Loot, INDEX_NONE, WeaponId);
        if (Loot.GearIndex != INDEX_NONE) continue;
        if (SurvivalItemHasQuality(Item) || WeaponId != INDEX_NONE)
        {
            if (!bPlaced)
            {
                auto Cell = Loot.ToPackCell();
                Cell.WeaponId = WeaponId;
                PendingLootItems.Add(Cell);
            }
        }
        else
        {
            FSurvivalLoot Grant;
            if (int32* Count = Grant.StackCount(Item))
            {
                *Count = Loot.Count;
                constexpr int32 AppraisalBonus[] = {0, 1, 2, 4};
                const int32 Appraisal = FMath::Clamp(Progression.Rank(ETraining::LootAppraisal), 0, 3);
                const int32 Bonus = bApplyAppraisal
                    ? (Progression.Rank(ETraining::Scavenging) - 1) / 25 + AppraisalBonus[Appraisal] : 0;
                Grant.Materials.Timber += FMath::Min(Bonus, Grant.Materials.Timber);
                Grant.Materials.Stone += FMath::Min(Bonus, Grant.Materials.Stone);
                Inventory.Add(Grant.Materials);
                Items.Add(Grant.Items);
            }
        }
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
    if (HealthPoints <= 0) return;
    if (HasOpenContainer())
    {
        CloseContainer();
        return;
    }
    if (bInventoryOpen) return;
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
        if (!Crate->IsDepleted())
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
    UpdateFirearmModel();
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

const TCHAR* ASurvivalCharacter::GetWeaponLabel(ESurvivalItem Weapon) const
{
    return GetInstanceLabel(Weapon, GetWeaponId(Weapon));
}

const TCHAR* ASurvivalCharacter::GetInstanceLabel(ESurvivalItem Weapon, int32 Id) const
{
    if (!WeaponItems.IsValidIndex(Id)) return SurvivalItemName(Weapon);
    return Arsenal[FirearmIndex(Weapon, WeaponItems[Id].Firearm)].Label;
}

int32 ASurvivalCharacter::WeaponCapacity(ESurvivalItem Family) const
{
    const int32 Id = Toolbelt[ActiveHotbarSlot].WeaponId;
    const int32 Model = WeaponItems.IsValidIndex(Id) ? WeaponItems[Id].Firearm : INDEX_NONE;
    return Arsenal[FirearmIndex(Family, Model)].Capacity * (HasWeaponMod(Family, ExtendedMagazine) ? 2 : 1);
}

namespace
{
struct FActionRange { float Start; float End; };

// Seconds in the source's combined take, measured from its first time code.
FActionRange ActionRange(int32 Model, bool bReload)
{
    if (Model == 3) return bReload ? FActionRange{.60f, 7.25f} : FActionRange{0, .38f};
    if (Model == 6) return bReload ? FActionRange{9.55f, 12.65f} : FActionRange{.82f, 1.15f};
    return {0, 0};
}
}

float ASurvivalCharacter::ActionDuration(bool bReload, UAnimSequence* Clip, float Fallback) const
{
    const auto Range = ActionRange(EquippedFirearm, bReload);
    if (Clip && Range.End > Range.Start) return Range.End - Range.Start;
    return Clip ? Clip->GetPlayLength() : Fallback;
}

void ASurvivalCharacter::PlaySecondaryClip(const TCHAR* ClipRole, float Start, float Rate)
{
    if (!SecondaryRig || EquippedFirearm < 2) return;
    const FString Path = FString::Printf(TEXT("/Game/Art/Crossroads/Arsenal/%s/%s1.%s1"),
        Arsenal[EquippedFirearm].Asset, ClipRole, ClipRole);
    if (auto* Sequence = LoadObject<UAnimSequence>(nullptr, *Path, nullptr, LOAD_NoWarn))
    {
        SecondaryRig->PlayAnimation(Sequence, false);
        SecondaryRig->SetPosition(Start, false);
        SecondaryRig->SetPlayRate(Rate);
    }
}

void ASurvivalCharacter::UpdateFirearmModel()
{
    const auto& Active = Toolbelt[ActiveHotbarSlot];
    const bool bWeapon = Active.Item == ESurvivalItem::Sidearm || Active.Item == ESurvivalItem::Rifle;
    if (SecondaryRig) SecondaryRig->SetHiddenInGame(!bWeapon || bThirdPerson || bInventoryOpen || bBuildMode || HealthPoints <= 0);
    if (!bWeapon || !WeaponItems.IsValidIndex(Active.WeaponId)) return;
    const int32 Model = FirearmIndex(Active.Item, WeaponItems[Active.WeaponId].Firearm);
    if (Model == EquippedFirearm) return;
    EquippedFirearm = Model;
    if (SecondaryRig) { SecondaryRig->DestroyComponent(); SecondaryRig = nullptr; }
    const bool bPistol = Active.Item == ESurvivalItem::Sidearm;
    UStaticMeshComponent* WorldPart = bPistol ? (Sidearm ? Sidearm->GetWeaponMesh() : nullptr) : RifleWorldMesh.Get();
    UStaticMesh* Surface = FirearmWorldMesh(Model);
    if (!Surface)
        Surface = LoadObject<UStaticMesh>(nullptr, bPistol ?
            TEXT("/Game/Art/Crossroads/Building/SM_CrossroadsSidearm.SM_CrossroadsSidearm") :
            TEXT("/Game/Art/Crossroads/SniperSurface/SM_CrossroadsSniper.SM_CrossroadsSniper"));
    if (WorldPart && Surface)
    {
        const auto Bounds = Surface->GetBounds();
        const double Scale = (bPistol ? 24.0 : 85.0) / FMath::Max(1., 2 * Bounds.BoxExtent.GetMax());
        const FRotator Rotation(0, (bPistol ? 0 : 90) - (Bounds.BoxExtent.Y > Bounds.BoxExtent.X ? 90 : 0), 0);
        WorldPart->SetStaticMesh(Surface);
        WorldPart->SetRelativeScale3D(FVector(Scale));
        WorldPart->SetRelativeRotation(Rotation);
        WorldPart->SetRelativeLocation((bPistol ? FVector(6,0,1) : FVector(0,23,0)) - Rotation.RotateVector(Bounds.Origin * Scale));
    }
    const auto LoadBase = [&] { if (bPistol) LoadSidearmViewmodel(); else LoadRifleViewmodel(); };
    if (Model < 2)
    {
        if (bPistol) CurrentViewmodelAnimation = nullptr;
        else CurrentRifleAnimation = nullptr;
        LoadBase();
        return;
    }
    const auto Path = [Model](const TCHAR* Name)
    {
        return FString::Printf(TEXT("/Game/Art/Crossroads/Arsenal/%s/%s.%s"), Arsenal[Model].Asset, Name, Name);
    };
    auto* Rig = LoadObject<USkeletalMesh>(nullptr, *Path(TEXT("Rig0")), nullptr, LOAD_NoWarn);
    const auto Clip = [&](const TCHAR* Name) { return LoadObject<UAnimSequence>(nullptr, *Path(Name), nullptr, LOAD_NoWarn); };
    UAnimSequence* Presentation = Clip(TEXT("Presentation0"));
    if (!Rig || !Presentation) { LoadBase(); return; }
    auto* View = bPistol ? SidearmViewmodel.Get() : RifleViewmodel.Get();
    View->EmptyOverrideMaterials();
    View->SetSkeletalMesh(Rig);
    RefreshViewmodelGear(View);
    const auto Bounds = Rig->GetBounds();
    const double Scale = (bPistol ? 65.0 : 100.0) / FMath::Max(1.0, Bounds.BoxExtent.GetMax() * 2);
    View->SetRelativeScale3D(FVector(Scale));
    View->SetRelativeRotation(FRotator::ZeroRotator);
    const FVector Hip = FVector(40, 8, -16) - Bounds.Origin * Scale;
    View->SetRelativeLocation(Hip);
    if (bPistol)
    {
        ViewmodelHip = Hip;
        ViewmodelAim = Hip + FVector(0,-8,5);
        SidearmHipRotation = SidearmAimRotation = FQuat::Identity;
        ViewmodelIdle = Presentation;
        ViewmodelWalk = Clip(TEXT("Walk"));
        ViewmodelFire = Clip(TEXT("Fire"));
        ViewmodelReload = Clip(TEXT("Reload"));
        ViewmodelReloadEmpty = ViewmodelReload;
        bViewmodelReady = true;
        CurrentViewmodelAnimation = nullptr;
    }
    else
    {
        RifleHip = Hip;
        RifleAim = Hip + FVector(0,-8,5);
        RifleHold = Presentation;
        RifleFire = Clip(TEXT("Fire"));
        RifleReload = Clip(TEXT("Reload"));
        CurrentRifleAnimation = nullptr;
    }
    if (auto* Other = LoadObject<USkeletalMesh>(nullptr, *Path(TEXT("Rig1")), nullptr, LOAD_NoWarn))
    {
        SecondaryRig = NewObject<USkeletalMeshComponent>(this);
        SecondaryRig->SetupAttachment(View);
        SecondaryRig->SetSkeletalMesh(Other);
        RefreshViewmodelGear(SecondaryRig);
        SecondaryRig->SetOnlyOwnerSee(true);
        SecondaryRig->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        SecondaryRig->SetCastShadow(false);
        SecondaryRig->RegisterComponent();
        SecondaryRig->PlayAnimation(Clip(TEXT("Presentation1")), false);
        if (Model == 3) SecondaryRig->SetPlayRate(0.f);
        SecondaryRig->SetHiddenInGame(bThirdPerson || bInventoryOpen || bBuildMode || HealthPoints <= 0);
    }
}

void ASurvivalCharacter::LoadSidearmViewmodel()
{
    constexpr TCHAR MeshPath[] = TEXT("/Game/Art/Crossroads/DressedSidearm/SK_SidearmViewmodel.SK_SidearmViewmodel");
    USkeletalMesh* Arms = LoadObject<USkeletalMesh>(nullptr, MeshPath, nullptr, LOAD_NoWarn);
    if (!Arms)
    {
        return;
    }

    SidearmViewmodel->SetSkeletalMesh(Arms);
    RefreshViewmodelGear(SidearmViewmodel);
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
        const FString Path = FString::Printf(TEXT("/Game/Art/Crossroads/DressedSidearm/%s.%s"), Name, Name);
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
        const bool bLoopClip = bLoop && EquippedFirearm != 3;
        SidearmViewmodel->PlayAnimation(Sequence, bLoopClip);
        const bool bReloadClip = Sequence == ViewmodelReload.Get() || Sequence == ViewmodelReloadEmpty.Get();
        const bool bFireClip = Sequence == ViewmodelFire.Get();
        const auto Range = ActionRange(EquippedFirearm, bReloadClip);
        const float Start = (bFireClip || bReloadClip) ? Range.Start : 0;
        const float Rate = EquippedFirearm == 3 && !bFireClip && !bReloadClip ? 0.f : 1.f;
        SidearmViewmodel->SetPosition(Start, false);
        SidearmViewmodel->SetPlayRate(Rate);
        PlaySecondaryClip(bReloadClip ? TEXT("Reload") : bFireClip ? TEXT("Fire") : TEXT("Presentation"), Start, Rate);
    }
}

void ASurvivalCharacter::UpdateSidearmAnimation()
{
    const double Now = GetWorld()->GetTimeSeconds();
    if (IsSidearmSelected() && IsReloading() && Now >= ReloadCompleteAt)
    {
        const int32 Transfer = FMath::Min(Items.Rounds, FMath::Max(0, WeaponCapacity(ESurvivalItem::Sidearm) - GetAmmo()));
        WeaponItems[Toolbelt[ActiveHotbarSlot].WeaponId].Ammo += Transfer;
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
    return bAimHeld && bHasSidearm && GetWeaponAssembly(ESurvivalItem::Sidearm).IsComplete() && IsSidearmSelected() && !bThirdPerson && !bBuildMode && !bInventoryOpen && !IsReloading() && HealthPoints > 0;
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
    return bAimHeld && RifleHold && bHasRifle && GetWeaponAssembly(ESurvivalItem::Rifle).IsComplete() && IsRifleSelected() && !bThirdPerson
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
    const bool bThermal = HasWeaponMod(GetHotbarItem(ActiveHotbarSlot), ThermalSight) || HasWeaponMod(GetHotbarItem(ActiveHotbarSlot), IRSight);
    const bool bMagnified = bThermal || EquippedFirearm == 1 || HasWeaponMod(ESurvivalItem::Rifle, VariableScope) || HasWeaponMod(ESurvivalItem::Rifle, IRSight);
    const bool bScopeAiming = bMagnified && IsAimingRifle() && IsLocallyControlled() && GetWorld()->GetTimeSeconds() >= RifleReadyAt;
    const float PreviousAim = AimBlend;
    AimBlend = FMath::FInterpConstantTo(AimBlend, bPistolAiming ? 1.0f : 0.0f, DeltaSeconds, 7.0f);
    ScopeBlend = FMath::FInterpConstantTo(ScopeBlend, bScopeAiming ? 1.0f : 0.0f, DeltaSeconds, 6.0f);

    const float PistolAlpha = FMath::SmoothStep(0.0f, 1.0f, AimBlend);
    const float RifleAlpha = FMath::SmoothStep(0.0f, 1.0f, ScopeBlend);
    const auto* ControllerPC = Cast<APlayerController>(Controller);
    const bool bVariable = HasWeaponMod(GetHotbarItem(ActiveHotbarSlot), VariableScope);
    if (bVariable && ControllerPC && (PistolAlpha > .5f || RifleAlpha > .5f) && ControllerPC->WasInputKeyJustPressed(EKeys::Z))
        bHighOpticZoom = !bHighOpticZoom;
    ScopeCapture->PostProcessSettings.bOverride_ColorSaturation = bThermal;
    ScopeCapture->PostProcessSettings.ColorSaturation = FVector4(0,0,0,1);
    ScopeCapture->PostProcessSettings.bOverride_ColorContrast = bThermal;
    ScopeCapture->PostProcessSettings.ColorContrast = FVector4(1.3,1.3,1.3,1);
    ScopeCapture->FOVAngle = bVariable ? (bHighOpticZoom ? 6.f : 15.f) : 20.f;
    FirstPersonCamera->SetFieldOfView(90.0f - (bVariable ? (bHighOpticZoom ? 65.f : 40.f) : 16.f) * PistolAlpha - 6.0f * RifleAlpha);
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
        RifleADS = FMath::FInterpConstantTo(RifleADS, IsAimingRifle() ? 1.f : 0.f, DeltaSeconds, 6.f);
        FVector Pose = FMath::Lerp(RifleHip, RifleAim, RifleADS);
        if (!RifleReload && IsRifleReloading())
        {
            const double Duration = HasWeaponMod(ESurvivalItem::Rifle, QuickMagazine) ? 1.8 / 1.35 : 1.8;
            const double Phase = FMath::Clamp((RifleReloadAt - GetWorld()->GetTimeSeconds()) / Duration, 0., 1.);
            Pose.Z -= 12 * FMath::Sin(PI * Phase);
        }
        if (!RifleFire && RifleReadyAt > GetWorld()->GetTimeSeconds())
            Pose.X -= 4 * FMath::Clamp((RifleReadyAt - GetWorld()->GetTimeSeconds()) / .14, 0., 1.);
        RifleViewmodel->SetRelativeLocation(Pose);
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
    if (HealthPoints <= 0 || bInventoryOpen || !bHasSidearm || !GetWeaponAssembly(ESurvivalItem::Sidearm).IsComplete() || !IsSidearmSelected() || IsReloading() || Now < NextShotAt
        || GetAmmo() >= WeaponCapacity(ESurvivalItem::Sidearm) || Items.Rounds <= 0)
    {
        return;
    }
    UAnimSequence* Clip = GetAmmo() == 0 && ViewmodelReloadEmpty ? ViewmodelReloadEmpty.Get() : ViewmodelReload.Get();
    if (!Clip)
    {
        Clip = ViewmodelReloadEmpty.Get();
    }
    const float Rate = HasWeaponMod(ESurvivalItem::Sidearm, QuickMagazine) ? 1.35f : 1.0f;
    ReloadCompleteAt = Now + ActionDuration(true, Clip, 1.4f) / Rate;
    PlaySidearmAnimation(Clip, false);
    SidearmViewmodel->SetPlayRate(Rate);
    if (SecondaryRig) SecondaryRig->SetPlayRate(Rate);
}

bool ASurvivalCharacter::ShowHitMarker() const
{
    return GetWorld()->GetTimeSeconds() < HitMarkerUntil;
}

void ASurvivalCharacter::PrimaryAction()
{
    if (HealthPoints <= 0 || bInventoryOpen || HasOpenContainer())
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
        UseToolbeltSlot(ActiveHotbarSlot);
        return;
    }
    const double Now = GetWorld()->GetTimeSeconds();
    if (!bHasSidearm || !GetWeaponAssembly(ESurvivalItem::Sidearm).IsComplete() || GetAmmo() <= 0 || IsReloading() || Now < NextShotAt)
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

    --WeaponItems[Toolbelt[ActiveHotbarSlot].WeaponId].Ammo;
    NextShotAt = Now + ActionDuration(false, ViewmodelFire, .23f);
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
        Sidearm->PlayShotSound(HasWeaponMod(ESurvivalItem::Sidearm, Suppressor) ? .25f : 1.f);
    }
    AddControllerPitchInput((bAimedShot ? -0.38f : -0.65f) * WeaponRecoil(ESurvivalItem::Sidearm));
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
        Round->SetRifleRound(GetWeaponAssembly(ESurvivalItem::Rifle).GetDamage());
    }
    return Round != nullptr;

}

void ASurvivalCharacter::LoadRifleViewmodel()
{
    RifleWorldMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,
        TEXT("/Game/Art/Crossroads/SniperSurface/SM_CrossroadsSniper.SM_CrossroadsSniper")));
    USkeletalMesh* Arms = LoadObject<USkeletalMesh>(nullptr,
        TEXT("/Game/Art/Crossroads/DressedSniper/SK_SniperViewmodel.SK_SniperViewmodel"));
    if (!Arms)
    {
        return;
    }
    RifleViewmodel->SetSkeletalMesh(Arms);
    RefreshViewmodelGear(RifleViewmodel);
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
        const FString Path = FString::Printf(TEXT("/Game/Art/Crossroads/DressedSniper/%s.%s"), Name, Name);
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
    if (!bHasRifle || !GetWeaponAssembly(ESurvivalItem::Rifle).IsComplete() || GetRifleAmmo() <= 0 || IsRifleReloading() || Now < RifleReadyAt)
    {
        return;
    }
    const FVector Origin = FirstPersonCamera->GetComponentTransform().TransformPosition(FVector(74, 0, -13));
    if (!FireRound(Origin, 12000.0, true))
    {
        return;
    }
    --WeaponItems[Toolbelt[ActiveHotbarSlot].WeaponId].Ammo;
    CurrentRifleAnimation = nullptr;
    RifleReadyAt = Now + ActionDuration(false, RifleFire, EquippedFirearm >= 5 ? .14f : 1.f) / (HasWeaponMod(ESurvivalItem::Rifle, FasterBolt) ? 1.25 : 1.0);
    AddControllerPitchInput(-1.35f * WeaponRecoil(ESurvivalItem::Rifle));
    if (USoundBase* Sound = LoadObject<USoundBase>(nullptr,
        TEXT("/Game/Audio/Weapons/SW_SidearmShot.SW_SidearmShot"), nullptr, LOAD_NoWarn))
    {
        UGameplayStatics::PlaySoundAtLocation(this, Sound, Origin, HasWeaponMod(ESurvivalItem::Rifle, Suppressor) ? .25f : 1.0f);
    }
}

void ASurvivalCharacter::ReloadRifle()
{
    const double Now = GetWorld()->GetTimeSeconds();
    if (bHasRifle && GetWeaponAssembly(ESurvivalItem::Rifle).IsComplete() && HealthPoints > 0 && !bInventoryOpen && !IsRifleReloading()
        && GetRifleAmmo() < WeaponCapacity(ESurvivalItem::Rifle) && Items.RifleRounds > 0 && Now >= RifleReadyAt)
    {
        CurrentRifleAnimation = nullptr;
        RifleReloadAt = Now + ActionDuration(true, RifleReload, 1.8f) / (HasWeaponMod(ESurvivalItem::Rifle, QuickMagazine) ? 1.35 : 1.0);
    }
}

void ASurvivalCharacter::UpdateRifle()
{
    const double Now = GetWorld()->GetTimeSeconds();
    if (IsRifleSelected() && IsRifleReloading() && Now >= RifleReloadAt)
    {
        const int32 Transfer = FMath::Min(FMath::Max(0, WeaponCapacity(ESurvivalItem::Rifle) - GetRifleAmmo()), Items.RifleRounds);
        Items.RifleRounds -= Transfer;
        WeaponItems[Toolbelt[ActiveHotbarSlot].WeaponId].Ammo += Transfer;
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
        RifleViewmodel->PlayAnimation(Desired, Desired == RifleHold.Get() && EquippedFirearm < 5);
        const bool bFire = Desired == RifleFire.Get();
        const bool bReload = Desired == RifleReload.Get();
        const float Rate = bFire && HasWeaponMod(ESurvivalItem::Rifle, FasterBolt) ? 1.25f :
            bReload && HasWeaponMod(ESurvivalItem::Rifle, QuickMagazine) ? 1.35f :
            (EquippedFirearm == 6 && !bFire && !bReload) ? 0.f : 1.f;
        const float Start = bFire || bReload ? ActionRange(EquippedFirearm, bReload).Start :
            EquippedFirearm == 6 ? .5f : 0.f;
        RifleViewmodel->SetPosition(Start, false);
        RifleViewmodel->SetPlayRate(Rate);
        PlaySecondaryClip(bReload ? TEXT("Reload") : bFire ? TEXT("Fire") : TEXT("Presentation"), Start, Rate);
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
        CloseContainer();
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
    InspectWeapon(ESurvivalItem::None);
    ShowWeaponPortrait(false);
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
    if (SurvivalItemHasQuality(Item))
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
    case ESurvivalItem::Sidearm:
    case ESurvivalItem::Rifle:
    {
        int32 Count = 0;
        for (const auto& Weapon : WeaponItems) Count += Weapon.Item == Item;
        return Count;
    }
    case ESurvivalItem::Food: return Items.Food;
    case ESurvivalItem::Water: return Items.Water;
    case ESurvivalItem::Bandage: return Items.Bandages;
    case ESurvivalItem::Timber: return Inventory.Timber;
    case ESurvivalItem::Stone: return Inventory.Stone;
    case ESurvivalItem::Cloth: return Items.Cloth;
    case ESurvivalItem::Rounds: return Items.Rounds;
    case ESurvivalItem::TimberDoor: return Items.TimberDoors;
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
        SelectedWeaponId = Pack[Slot].WeaponId;
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
    FSurvivalPackCell& Source = bFromBelt ? Toolbelt[From] : Pack[From];
    FSurvivalPackCell& Target = bToBelt ? Toolbelt[To] : Pack[To];
    if (Source.IsEmpty() || &Source == &Target ||
        (bToBelt && !CanQuickSlot(Source.Item)) ||
        (bFromBelt && !bToBelt && !Target.IsEmpty() && !CanQuickSlot(Target.Item)))
    {
        return false;
    }
    const int32 PreviousWeaponId = Toolbelt[ActiveHotbarSlot].WeaponId;
    Swap(Source, Target);
    if (bToBelt)
    {
        ActiveHotbarSlot = To;
        SelectToolbeltItem(To);
    }
    else
    {
        SetSelectedPackSlot(To);
    }
    CancelWeaponReloadOnSwitch(PreviousWeaponId);
    UpdateSidearmView();
    RefreshAttachments();
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
    const int32 Limit = Source.GearIndex == INDEX_NONE ? SurvivalItemMaxStack(Source.Item) : MAX_int32;
    int32 Moved = bHalf ? (Source.Count + 1) / 2 : Source.Count;
    if (Target.IsEmpty())
    {
        Target = Source;
        Target.Count = Moved;
    }
    else if (Target.Item == Source.Item && Target.GearIndex == Source.GearIndex && Limit > 1)
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

bool ASurvivalCharacter::SwapWeaponPart(int32 PackSlot)
{
    if (!CanInspectWeapon(InspectedWeapon) || !WeaponItems.IsValidIndex(InspectedWeaponId) || !Pack.IsValidIndex(PackSlot) || Pack[PackSlot].IsEmpty())
    {
        return false;
    }
    FSurvivalPackCell& Cell = Pack[PackSlot];
    const int32 Slot = FSurvivalWeaponAssembly::PartIndex(Cell.Item, InspectedWeapon);
    if (Slot == INDEX_NONE)
    {
        return false;
    }
    // The removed part takes the cell the new part came from.
    FSurvivalWeaponAssembly& Assembly = WeaponItems[InspectedWeaponId].Assembly;
    Cell.Quality = Assembly.SwapPart(Slot, Cell.Quality);
    if (Cell.Quality == 0) Cell = {};
    RefreshAttachments();
    SetSelectedPackSlot(PackSlot);
    return true;
}

bool ASurvivalCharacter::RemoveWeaponPart(int32 PartSlot)
{
    if (!CanInspectWeapon(InspectedWeapon) || !WeaponItems.IsValidIndex(InspectedWeaponId) ||
        PartSlot < 0 || PartSlot >= FSurvivalWeaponAssembly::PartCount)
        return false;

    FSurvivalWeaponAssembly& Assembly = WeaponItems[InspectedWeaponId].Assembly;
    if (Assembly.GetPartQuality(PartSlot) == 0) return false;
    const int32 Cell = FindFreePackCell();
    if (Cell == INDEX_NONE) return false;
    Pack[Cell] = {static_cast<ESurvivalItem>(static_cast<int32>(FSurvivalWeaponAssembly::FirstPart(InspectedWeapon)) + PartSlot),
        1, Assembly.SwapPart(PartSlot, 0)};
    RefreshAttachments();
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
    while (!PendingLootItems.IsEmpty())
    {
        const int32 Slot = FindFreePackCell();
        if (Slot == INDEX_NONE) break;
        Pack[Slot] = PendingLootItems.Pop();
    }
    // OwnedGear includes worn garments. Only spare copies occupy backpack cells.
    int32 Remaining[UE_ARRAY_COUNT(SurvivalGear)]{};
    for (int32 Id : OwnedGear)
        if (Id >= 0 && Id < UE_ARRAY_COUNT(SurvivalGear)) ++Remaining[Id];
    for (int32 Id : WornGear)
        if (Id >= 0 && Id < UE_ARRAY_COUNT(SurvivalGear) && Remaining[Id] > 0) --Remaining[Id];
    for (auto& Cell : Pack)
    {
        if (Cell.GearIndex == INDEX_NONE) continue;
        Cell.Count = FMath::Min(Cell.Count, Remaining[Cell.GearIndex]);
        Remaining[Cell.GearIndex] -= Cell.Count;
        if (Cell.Count == 0) Cell = {};
    }
    for (int32 Id = 0; Id < UE_ARRAY_COUNT(Remaining); ++Id)
    {
        if (Remaining[Id] == 0) continue;
        FSurvivalPackCell* Existing = Pack.FindByPredicate([Id](const auto& Cell){ return Cell.GearIndex == Id; });
        if (Existing) Existing->Count += Remaining[Id];
        else
        {
            const int32 Slot = FindFreePackCell();
            if (Slot == INDEX_NONE) break;
            Pack[Slot].GearIndex = Id;
            Pack[Slot].Count = Remaining[Id];
        }
    }

    for (auto It = PendingGearTargets.CreateIterator(); It; ++It)
    {
        if (!Pack.IsValidIndex(It.Value()) || !Pack[It.Value()].IsEmpty())
        {
            It.RemoveCurrent();
            continue;
        }
        FSurvivalPackCell* Source = Pack.FindByPredicate([Id = It.Key()](const auto& Cell){ return Cell.GearIndex == Id; });
        if (!Source) continue;
        Pack[It.Value()] = *Source;
        Pack[It.Value()].Count = 1;
        if (--Source->Count == 0) *Source = {};
        It.RemoveCurrent();
    }
    for (const auto Weapon : {ESurvivalItem::Rifle, ESurvivalItem::Sidearm})
    {
        auto& SpareParts = Weapon == ESurvivalItem::Rifle ? Items.RifleParts : Items.SidearmParts;
        for (int32 Slot = 0; Slot < FSurvivalWeaponAssembly::PartCount; ++Slot)
        {
            const auto Part = static_cast<ESurvivalItem>(static_cast<int32>(FSurvivalWeaponAssembly::FirstPart(Weapon)) + Slot);
            while (SpareParts[Slot] > 0)
            {
                const int32 Index = FindFreePackCell();
                if (Index == INDEX_NONE) break;
                Pack[Index] = {Part, 1, FSurvivalWeaponAssembly::SpareQuality(Slot)};
                --SpareParts[Slot];
            }
        }
    }
    for (int32 Index = 0; Index < static_cast<int32>(ESurvivalItem::None); ++Index)
    {
        const ESurvivalItem Item = static_cast<ESurvivalItem>(Index);
        if (SurvivalItemHasQuality(Item) || IsWeaponLoot(Item))
        {
            continue;
        }
        int32 InPack = 0;
        for (const FSurvivalPackCell& Cell : Pack)
        {
            InPack += !Cell.IsEmpty() && Cell.Item == Item ? Cell.Count : 0;
        }
        const int32 Total = GetItemCount(Item);
        int32 OnBelt = 0;
        for (const FSurvivalPackCell& Cell : Toolbelt)
            if (Cell.Item == Item) OnBelt += Cell.Count;
        int32 Excess = FMath::Max(0, OnBelt - Total);
        for (FSurvivalPackCell& Cell : Toolbelt)
        {
            if (Excess == 0) break;
            if (Cell.Item != Item) continue;
            const int32 Removed = FMath::Min(Cell.Count, Excess);
            Cell.Count -= Removed;
            OnBelt -= Removed;
            Excess -= Removed;
            if (Cell.Count == 0) Cell = {};
        }
        const int32 PackTotal = Total - OnBelt;
        if (InPack < PackTotal)
        {
            AddToPack(Item, PackTotal - InPack, 0);
        }
        else if (InPack > PackTotal)
        {
            RemoveFromPack(Item, InPack - PackTotal);
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

void ASurvivalCharacter::UseToolbeltSlot(int32 Slot)
{
    if (Slot < 0 || Slot >= HotbarSize || !CanUseItem(Toolbelt[Slot].Item))
    {
        return;
    }
    FSurvivalPackCell& Cell = Toolbelt[Slot];
    UseItem(Cell.Item);
    if (--Cell.Count == 0) Cell = {};
}

void ASurvivalCharacter::ChooseHotbarSlot(int32 Slot)
{
    if (Slot < 0 || Slot >= HotbarSize || HealthPoints <= 0)
    {
        return;
    }
    if (bInventoryOpen && Pack.IsValidIndex(SelectedPackSlot) && CanQuickSlot(Pack[SelectedPackSlot].Item))
    {
        MoveMenuItem(false, SelectedPackSlot, true, Slot);
        return;
    }
    const int32 PreviousWeaponId = Toolbelt[ActiveHotbarSlot].WeaponId;
    ActiveHotbarSlot = Slot;
    CancelWeaponReloadOnSwitch(PreviousWeaponId);
    UpdateSidearmView();
    RefreshAttachments();
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
    case ESurvivalItem::RifleRounds: Items.RifleRounds += Delta; break;
    case ESurvivalItem::BuilderGuide: Items.BuilderGuides += Delta; break;
    case ESurvivalItem::ScavengerNotes: Items.ScavengerNotes += Delta; break;
    case ESurvivalItem::DressingSchematic: Items.DressingSchematics += Delta; break;
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
    if (HasOpenContainer())
    {
        TakeContainerLoot();
        return;
    }
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

void ASurvivalCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(ASurvivalCharacter, OwnedGear, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(ASurvivalCharacter, LootGameStage, COND_OwnerOnly);
    DOREPLIFETIME(ASurvivalCharacter, WornGear);
}

int32 ASurvivalCharacter::GearCount(int32 Index) const
{
    int32 Count = 0;
    for (int32 Id : OwnedGear) Count += (Id == Index);
    return Count;
}

void ASurvivalCharacter::ServerSetGearEquipped_Implementation(int32 Index, bool bEquip)
{
    if (HealthPoints <= 0 || !OwnedGear.Contains(Index) || Index < 0 || Index >= UE_ARRAY_COUNT(SurvivalGear))
        return;
    if (WornGear.Contains(Index) == bEquip) return;
    WornGear.RemoveAll([Index](int32 Other) {
        return FCString::Strcmp(SurvivalGear[Other].Slot, SurvivalGear[Index].Slot) == 0;
    });
    if (bEquip) WornGear.Add(Index);
    RefreshGear();
    ForceNetUpdate();
}

void ASurvivalCharacter::RefreshGear()
{
    RefreshViewmodelGear(SidearmViewmodel);
    RefreshViewmodelGear(RifleViewmodel);
    if (SecondaryRig) RefreshViewmodelGear(SecondaryRig);
    for (UStaticMeshComponent* Part : GearMeshes)
        if (Part) Part->DestroyComponent();
    GearMeshes.Reset();
    struct FFit { const TCHAR* Slot; const TCHAR* Bone; FVector Size; FVector Offset; };
    static const FFit Fits[] = {
        {TEXT("Helmet"), TEXT("Head"), {25,25,20}, {0,0,16}},
        {TEXT("Face"), TEXT("Head"), {16,21,20}, {7,0,7}},
        {TEXT("Eyes"), TEXT("Head"), {12,22,9}, {9,0,12}},
        {TEXT("Torso"), TEXT("Spine1"), {34,52,54}, {0,0,3}},
        {TEXT("Body"), TEXT("Hips"), {38,54,130}, {0,0,-10}},
        {TEXT("Vest"), TEXT("Spine1"), {35,47,43}, {0,0,4}},
        {TEXT("Legs"), TEXT("Hips"), {32,43,85}, {0,0,-36}},
        {TEXT("Belt"), TEXT("Hips"), {32,42,9}, {0,0,2}},
        {TEXT("Hands"), TEXT("RightHand"), {14,12,18}, {0,0,0}}
    };
    for (int32 Index : WornGear)
    {
        if (Index < 0 || Index >= UE_ARRAY_COUNT(SurvivalGear)) continue;
        const auto& Gear = SurvivalGear[Index];
        const FFit* Fit = Algo::FindByPredicate(Fits, [&Gear](const FFit& F){ return FCString::Strcmp(F.Slot, Gear.Slot) == 0; });
        if (!Fit) continue;
        const TCHAR* MeshId = Gear.MeshOverride ? Gear.MeshOverride : Gear.Id;
        FString Path = FString::Printf(TEXT("/Game/Art/Equipment/%s/SM_%s.SM_%s"), MeshId, MeshId, MeshId);
        UStaticMesh* GearMesh = LoadObject<UStaticMesh>(nullptr, *Path);
        if (!GearMesh) continue;
        UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
        Part->SetStaticMesh(GearMesh);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetCanEverAffectNavigation(false);
        Part->RegisterComponent();
        const FVector Scale = Fit->Size / (GearMesh->GetBounds().BoxExtent * 2).ComponentMax(FVector(0.01));
        const FQuat Rotation = GetActorQuat();
        Part->SetWorldTransform(FTransform(Rotation, GetMesh()->GetSocketLocation(Fit->Bone)
            + Rotation.RotateVector(Fit->Offset - GearMesh->GetBounds().Origin * Scale), Scale));
        Part->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepWorldTransform, Fit->Bone);
        Part->SetOwnerNoSee(!bThirdPerson && !bCharacterPortrait);
        GearMeshes.Add(Part);
    }
    if (bInventoryOpen) ShowWeaponPortrait(InspectedWeapon != ESurvivalItem::None, bCharacterPortrait);
    else RefreshAttachments();
}

void ASurvivalCharacter::RefreshViewmodelGear(USkeletalMeshComponent* Viewmodel)
{
    const USkeletalMesh* ViewmodelMesh = Viewmodel->GetSkeletalMeshAsset();
    if (!IsLocallyControlled() || !ViewmodelMesh) return;

    const FSurvivalGearDefinition* OuterLayer = nullptr;
    const FSurvivalGearDefinition* BodyLayer = nullptr;
    const FSurvivalGearDefinition* Gloves = nullptr;
    for (const int32 Index : WornGear)
    {
        if (Index < 0 || Index >= UE_ARRAY_COUNT(SurvivalGear)) continue;
        const auto& Gear = SurvivalGear[Index];
        if (FCString::Strcmp(Gear.Slot, TEXT("Torso")) == 0 && Gear.ViewmodelColor.A > 0.0f)
            OuterLayer = &Gear;
        else if (FCString::Strcmp(Gear.Slot, TEXT("Body")) == 0)
            BodyLayer = &Gear;
        else if (FCString::Strcmp(Gear.Slot, TEXT("Hands")) == 0)
            Gloves = &Gear;
    }

    UMaterialInterface* Cloth = nullptr;
    const auto& Materials = ViewmodelMesh->GetMaterials();
    for (int32 Index = 0; Index < Materials.Num(); ++Index)
    {
        const auto& Slot = Materials[Index];
        const FString Name = Slot.MaterialSlotName.ToString();
        const FSurvivalGearDefinition* Layer = nullptr;
        if (Name.Contains(TEXT("ViewmodelSleeves")) || Name.Contains(TEXT("Cloths"))) Layer = OuterLayer ? OuterLayer : BodyLayer;
        else if (Name.Contains(TEXT("Gloves"))) Layer = Gloves;
        else continue;

        if (!Layer || Layer->ViewmodelColor.A <= 0.0f)
        {
            Viewmodel->SetMaterial(Index, nullptr);
            continue;
        }
        if (!Cloth)
            Cloth = LoadObject<UMaterialInterface>(nullptr,
                TEXT("/Game/Art/Crossroads/DressedSidearm/M_ViewmodelCloth.M_ViewmodelCloth"), nullptr, LOAD_NoWarn);
        if (!Cloth) return;
        auto* Material = Cast<UMaterialInstanceDynamic>(Viewmodel->GetMaterial(Index));
        if (!Material)
            Material = Viewmodel->CreateDynamicMaterialInstance(Index, Cloth);
        if (Material)
        {
            Material->SetVectorParameterValue(TEXT("GearColor"), Layer->ViewmodelColor);
            Material->SetScalarParameterValue(TEXT("Camo"), Layer->ViewmodelCamo);
        }
    }
}

void ASurvivalCharacter::ShowWeaponPortrait(bool bWeapon, bool bCharacter)
{
    bCharacterPortrait = bInventoryOpen && bCharacter;
    const bool bCapture = bInventoryOpen && (bWeapon || bCharacterPortrait);
    PortraitCapture->bCaptureEveryFrame = bCapture;
    GetMesh()->SetOwnerNoSee(!bThirdPerson && !bCharacterPortrait);
    for (UStaticMeshComponent* Part : GearMeshes) Part->SetOwnerNoSee(!bThirdPerson && !bCharacterPortrait);
    InspectionWeapon->SetVisibility(bInventoryOpen && bWeapon);
    if (!bCapture) { RefreshAttachments(); return; }
    if (!PortraitTexture)
    {
        PortraitTexture = NewObject<UTextureRenderTarget2D>(this);
        PortraitTexture->ClearColor = FLinearColor(0.035f,0.045f,0.05f);
        PortraitTexture->InitCustomFormat(384, 512, PF_B8G8R8A8, false);
        PortraitCapture->TextureTarget = PortraitTexture;
    }
    PortraitCapture->ClearShowOnlyComponents();
    FVector Focus = GetMesh()->Bounds.Origin;
    double Distance = 225;
    const int32 Id = GetWeaponId(InspectedWeapon);
    UStaticMesh* PreviewSurface = WeaponItems.IsValidIndex(Id) ? FirearmWorldMesh(WeaponItems[Id].Firearm) : nullptr;
    if (!PreviewSurface && bWeapon)
        PreviewSurface = LoadObject<UStaticMesh>(nullptr, InspectedWeapon == ESurvivalItem::Sidearm ?
            TEXT("/Game/Art/Crossroads/Building/SM_CrossroadsSidearm.SM_CrossroadsSidearm") :
            TEXT("/Game/Art/Crossroads/SniperSurface/SM_CrossroadsSniper.SM_CrossroadsSniper"));
    if (bWeapon && PreviewSurface)
    {
        InspectionWeapon->SetStaticMesh(PreviewSurface);
        InspectionWeapon->SetRelativeRotation(FRotator(0,90,0));
        PortraitCapture->ShowOnlyComponent(InspectionWeapon);
        Focus = InspectionWeapon->Bounds.Origin;
        Distance = FMath::Max(25., InspectionWeapon->Bounds.SphereRadius * 3.5);
    }
    else if (bCharacterPortrait)
    {
        PortraitCapture->ShowOnlyComponent(GetMesh());
        for (UStaticMeshComponent* Part : GearMeshes) PortraitCapture->ShowOnlyComponent(Part);
    }
    FVector Direction = (GetActorForwardVector() + .24 * GetActorRightVector()).GetSafeNormal();
    if (bCharacterPortrait) Direction = Direction.RotateAngleAxis(CharacterPortraitYaw, FVector::UpVector);
    PortraitCapture->FOVAngle = 45;
    PortraitCapture->SetWorldLocationAndRotation(Focus + Direction * Distance, (-Direction).Rotation());
    RefreshAttachments();
    PortraitCapture->CaptureSceneDeferred();
}

void ASurvivalCharacter::RotateCharacterPortrait(float Degrees)
{
    if (!bCharacterPortrait) return;
    CharacterPortraitYaw = FMath::UnwindDegrees(CharacterPortraitYaw + Degrees);
    const FVector Direction = (GetActorForwardVector() + .24 * GetActorRightVector()).GetSafeNormal()
        .RotateAngleAxis(CharacterPortraitYaw, FVector::UpVector);
    const FVector Focus = GetMesh()->Bounds.Origin;
    PortraitCapture->SetWorldLocationAndRotation(Focus + Direction * 225.0, (-Direction).Rotation());
}

bool ASurvivalCharacter::CanQuickSlot(ESurvivalItem Item)
{
    return Item == ESurvivalItem::Rifle || Item == ESurvivalItem::Sidearm || Item == ESurvivalItem::Food
        || Item == ESurvivalItem::Water || Item == ESurvivalItem::Bandage || Item == ESurvivalItem::FieldDressing;
}

bool ASurvivalCharacter::SplitSelection()
{
    const int32 From = SelectedPackSlot;
    if (!bInventoryOpen || !Pack.IsValidIndex(From) || Pack[From].Count < 2) return false;
    const int32 To = FindFreePackCell();
    return To != INDEX_NONE && MovePackStack(From, To, true);
}

void ASurvivalCharacter::ServerDropGear_Implementation(int32 Index)
{
    if (HealthPoints <= 0 || !OwnedGear.Contains(Index)) return;
    FSurvivalLoot Loot;
    Loot.GearIndex = Index;
    if (!ASurvivalSupplyCrate::Drop(*this, Loot)) return;
    OwnedGear.RemoveSingle(Index);
    if (!OwnedGear.Contains(Index) && WornGear.Remove(Index)) RefreshGear();
}

bool ASurvivalCharacter::CanDropSelection() const
{
    FSurvivalLoot Loot;
    return HasAuthority() && bInventoryOpen && Pack.IsValidIndex(SelectedPackSlot)
        && !Pack[SelectedPackSlot].IsEmpty() && !SurvivalItemHasQuality(Pack[SelectedPackSlot].Item)
        && Loot.StackCount(Pack[SelectedPackSlot].Item);
}

void ASurvivalCharacter::DropSelection()
{
    if (!CanDropSelection()) return;
    const FSurvivalPackCell Cell = Pack[SelectedPackSlot];
    FSurvivalLoot Loot;
    const int32 Amount = FMath::Min(Cell.Count, GetItemCount(Cell.Item));
    *Loot.StackCount(Cell.Item) = Amount;
    if (Amount > 0 && ASurvivalSupplyCrate::Drop(*this, Loot))
    {
        ChangeItemCount(Cell.Item, -Amount);
        Pack[SelectedPackSlot] = {};
        SyncPack();
        SetSelectedInventoryItem(ESurvivalItem::None);
    }
}

int32 ASurvivalCharacter::GetWeaponId(ESurvivalItem Weapon) const
{
    if (bInventoryOpen && InspectedWeapon == Weapon && WeaponItems.IsValidIndex(InspectedWeaponId))
        return InspectedWeaponId;
    if (bInventoryOpen && WeaponItems.IsValidIndex(SelectedWeaponId) && WeaponItems[SelectedWeaponId].Item == Weapon)
        return SelectedWeaponId;
    const auto& Active = Toolbelt[ActiveHotbarSlot];
    if (Active.Item == Weapon && WeaponItems.IsValidIndex(Active.WeaponId))
        return Active.WeaponId;
    return WeaponItems.IndexOfByPredicate([Weapon](const auto& Instance){ return Instance.Item == Weapon; });
}

void ASurvivalCharacter::InspectWeapon(ESurvivalItem Weapon)
{
    InspectedWeaponId = Weapon != ESurvivalItem::None ? GetWeaponId(Weapon) : INDEX_NONE;
    InspectedWeapon = Weapon;
}

void ASurvivalCharacter::SelectToolbeltItem(int32 Slot)
{
    SetSelectedInventoryItem(Toolbelt[Slot].Item);
    SelectedWeaponId = Toolbelt[Slot].WeaponId;
}

void ASurvivalCharacter::CancelWeaponReloadOnSwitch(int32 PreviousWeaponId)
{
    if (PreviousWeaponId == Toolbelt[ActiveHotbarSlot].WeaponId) return;
    ReloadCompleteAt = -1.0;
    RifleReloadAt = -1.0;
}

const FSurvivalWeaponAssembly& ASurvivalCharacter::GetWeaponAssembly(ESurvivalItem Weapon) const
{
    static const FSurvivalWeaponAssembly DefaultAssembly;
    const int32 Id = GetWeaponId(Weapon);
    return WeaponItems.IsValidIndex(Id) ? WeaponItems[Id].Assembly : DefaultAssembly;
}

int32 ASurvivalCharacter::GetWeaponQuality(int32 WeaponId) const
{
    return WeaponItems.IsValidIndex(WeaponId) ? WeaponItems[WeaponId].Assembly.GetQuality() : 0;
}

int32 ASurvivalCharacter::GetAmmo() const
{
    const int32 Id = Toolbelt[ActiveHotbarSlot].WeaponId;
    return IsSidearmSelected() && WeaponItems.IsValidIndex(Id) ? WeaponItems[Id].Ammo : 0;
}

int32 ASurvivalCharacter::GetRifleAmmo() const
{
    const int32 Id = Toolbelt[ActiveHotbarSlot].WeaponId;
    return IsRifleSelected() && WeaponItems.IsValidIndex(Id) ? WeaponItems[Id].Ammo : 0;
}

bool ASurvivalCharacter::CanInspectWeapon(ESurvivalItem Item) const
{
    if (!bInventoryOpen || HealthPoints <= 0) return false;
    if (Item == ESurvivalItem::Sidearm || FSurvivalWeaponAssembly::PartIndex(Item, ESurvivalItem::Sidearm) != INDEX_NONE)
        return bHasSidearm && !IsReloading();
    return (Item == ESurvivalItem::Rifle || SurvivalItemHasQuality(Item)) && CanModifyRifle();
}

bool ASurvivalCharacter::HasWeaponMod(ESurvivalItem Weapon, int32 Mod) const
{
    const int32 Id = GetWeaponId(Weapon);
    return WeaponItems.IsValidIndex(Id) && Mod >= 0 && Mod < UE_ARRAY_COUNT(WeaponMods)
        && (WeaponItems[Id].Mods & (1 << Mod));
}

bool ASurvivalCharacter::CanToggleWeaponMod(ESurvivalItem Weapon, int32 Mod) const
{
    if (!CanInspectWeapon(Weapon) || !WeaponItems.IsValidIndex(GetWeaponId(Weapon)) || Mod < 0 || Mod >= UE_ARRAY_COUNT(WeaponMods)) return false;
    const auto Family = Weapon == ESurvivalItem::Sidearm ? EWeaponFamily::Sidearm : EWeaponFamily::Rifle;
    return (WeaponMods[Mod].Families & static_cast<uint8>(Family)) &&
        (HasWeaponMod(Weapon, Mod) || GetWeaponQuality(GetWeaponId(Weapon)) >= MountQuality[static_cast<uint8>(WeaponMods[Mod].Slot)]);
}

void ASurvivalCharacter::ToggleWeaponMod(ESurvivalItem Weapon, int32 Mod)
{
    if (!CanToggleWeaponMod(Weapon, Mod)) return;
    const bool bRemove = HasWeaponMod(Weapon, Mod);
    auto& Installed = WeaponItems[GetWeaponId(Weapon)].Mods;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(WeaponMods); ++Index)
        if (WeaponMods[Index].Slot == WeaponMods[Mod].Slot)
            Installed &= ~(1u << Index);
    if (!bRemove) Installed |= 1u << Mod;
    SyncPack();
    RefreshAttachments();
}

float ASurvivalCharacter::WeaponRecoil(ESurvivalItem Weapon) const
{
    float Scale = 1;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(WeaponMods); ++Index)
        if (HasWeaponMod(Weapon,Index)) Scale *= WeaponMods[Index].RecoilMultiplier;
    return Scale;
}

void ASurvivalCharacter::RefreshAttachments()
{
    for (USpotLightComponent* Light : AttachmentLights)
        if (Light) Light->DestroyComponent();
    AttachmentLights.Reset();
    for (UStaticMeshComponent* Part : AttachmentMeshes)
        if (Part) Part->DestroyComponent();
    AttachmentMeshes.Reset();
    const bool bPreview = bInventoryOpen && InspectedWeapon != ESurvivalItem::None;
    const ESurvivalItem Weapon = bPreview ? InspectedWeapon : GetHotbarItem(ActiveHotbarSlot);
    const bool bRifle = Weapon == ESurvivalItem::Rifle;
    if ((!bRifle && Weapon != ESurvivalItem::Sidearm) || (bInventoryOpen && !bPreview) || bBuildMode || HealthPoints <= 0) return;
    UStaticMeshComponent* WorldMesh = bRifle ? RifleWorldMesh.Get() : Sidearm ? Sidearm->GetWeaponMesh() : nullptr;
    UStaticMeshComponent* Model = bPreview ? InspectionWeapon.Get() : WorldMesh;
    if (!Model || !Model->GetStaticMesh()) return;
    const int32 InstanceId = GetWeaponId(Weapon);
    if (bRifle && WeaponItems.IsValidIndex(InstanceId) && FirearmIndex(Weapon, WeaponItems[InstanceId].Firearm) == 1)
    {
        const bool bReplacement = HasWeaponMod(Weapon, VariableScope) || HasWeaponMod(Weapon, IRSight) || HasWeaponMod(Weapon, ThermalSight);
        const TCHAR* Surface = bReplacement ? TEXT("/Game/Art/Crossroads/Arsenal/SniperBare/SniperBare.SniperBare") :
            TEXT("/Game/Art/Crossroads/SniperSurface/SM_CrossroadsSniper.SM_CrossroadsSniper");
        if (auto* Receiver = LoadObject<UStaticMesh>(nullptr, Surface, nullptr, LOAD_NoWarn))
            Model->SetStaticMesh(Receiver);
        if (!bPreview && RifleViewmodel->DoesSocketExist(TEXT("Sniper_scope")))
        {
            if (bReplacement) RifleViewmodel->HideBoneByName(TEXT("Sniper_scope"), EPhysBodyOp::PBO_None);
            else RifleViewmodel->UnHideBoneByName(TEXT("Sniper_scope"));
        }
    }
    const FBoxSphereBounds Bounds = Model->GetStaticMesh()->GetBounds();
    const FVector Axis = Bounds.BoxExtent.X >= Bounds.BoxExtent.Y ? FVector::ForwardVector : FVector::RightVector;
    const double Half = FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y);
    const double Unit = 2 * Half / (bRifle ? 100.0 : 24.0);
    auto Mount = [&](const FVector& Size, double Along, double Height, const TCHAR* Asset = nullptr)
    {
        const bool bViewmodel = !bPreview && !bThirdPerson && (bRifle ? RifleHold != nullptr : bViewmodelReady);
        USceneComponent* Parent = Model;
        FName Bone = NAME_None;
        FVector Location = Bounds.Origin + Axis * (Along*Half) + FVector::UpVector * (Height*Bounds.BoxExtent.Z);
        FVector Dimensions = Size * Unit;
        if (bViewmodel)
        {
            Parent = bRifle ? RifleViewmodel.Get() : SidearmViewmodel.Get();
            Bone = bRifle ? SniperMuzzle : ViewmodelGripBone;
            Location = FVector((Along - (bRifle ? 1.0 : 0.0)) * (bRifle ? 44.3 : 12.0),0, Height*4.0)
                / FMath::Max(.001, Parent->GetComponentScale().GetAbsMax());
            Dimensions = Size / FMath::Max(.001, Parent->GetComponentScale().GetAbsMax());
            if (EquippedFirearm >= 2)
            {
                USkeletalMeshComponent* Rig = bRifle ? RifleViewmodel.Get() : SidearmViewmodel.Get();
                static const FName ReceiverBones[] = {NAME_None, NAME_None, TEXT("Body_015"), TEXT("J1"),
                    TEXT("Body_057"), TEXT("n69"), TEXT("n56"), TEXT("n177"), TEXT("n1072")};
                Bone = ReceiverBones[EquippedFirearm];
                if (!Rig->DoesSocketExist(Bone) && SecondaryRig && SecondaryRig->DoesSocketExist(Bone))
                {
                    Rig = SecondaryRig.Get();
                    Parent = Rig;
                }
                if (!Rig->DoesSocketExist(Bone)) Bone = NAME_None;
                const auto RigBounds = Rig->GetSkeletalMeshAsset()->GetBounds();
                Location = RigBounds.Origin + FVector(Along * RigBounds.BoxExtent.GetMax() * .7, 0,
                    Height * RigBounds.BoxExtent.Z * .6);
                if (Bone != NAME_None)
                    Location = Rig->GetSocketTransform(Bone, RTS_Component).InverseTransformPosition(Location);
            }
        }
        auto* Part = NewObject<UStaticMeshComponent>(this);
        Part->SetupAttachment(Parent, Bone);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetCanEverAffectNavigation(false);
        UStaticMesh* ArtMesh = Asset ? LoadObject<UStaticMesh>(nullptr, *FString::Printf(
            TEXT("/Game/Art/Crossroads/Arsenal/%s/%s.%s"), Asset, Asset, Asset), nullptr, LOAD_NoWarn) : nullptr;
        Part->SetStaticMesh(ArtMesh ? ArtMesh : LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        if (!ArtMesh)
        {
            if (UMaterialInterface* Finish = LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Crossroads/Building/M_DefenseSteel.M_DefenseSteel")))
                Part->SetMaterial(0,Finish);
        }
        Part->SetRelativeLocation(Location);
        Part->SetRelativeRotation(bViewmodel ? FRotator::ZeroRotator : Axis.Rotation());
        if (ArtMesh)
        {
            const auto ArtBounds = ArtMesh->GetBounds();
            const FVector Extent = ArtBounds.BoxExtent;
            const FRotator Orient = Extent.Z >= Extent.X && Extent.Z >= Extent.Y ? FRotator(90,0,0) :
                Extent.Y > Extent.X ? FRotator(0,90,0) : FRotator::ZeroRotator;
            const FQuat Rotation = Part->GetRelativeRotation().Quaternion() * Orient.Quaternion();
            const double Scale = Dimensions.GetMax() / FMath::Max(.001, 2 * Extent.GetMax());
            Part->SetRelativeScale3D(FVector(Scale));
            Part->SetRelativeRotation(Rotation);
            Part->SetRelativeLocation(Location - Rotation.RotateVector(ArtBounds.Origin * Scale));
        }
        else Part->SetRelativeScale3D(Dimensions / 100);
        Part->SetVisibleInSceneCaptureOnly(bPreview);
        Part->SetOnlyOwnerSee(bViewmodel);
        Part->RegisterComponent();
        AttachmentMeshes.Add(Part);
        if (bPreview) PortraitCapture->ShowOnlyComponent(Part);
    };
    for (int32 Index=0; Index<UE_ARRAY_COUNT(WeaponMods); ++Index)
        if (HasWeaponMod(Weapon,Index))
        {
            const auto& Mod = WeaponMods[Index];
            const TCHAR* Art = nullptr;
            if (Index == Suppressor) Art = bRifle ? TEXT("SuppressorRifle") : TEXT("SuppressorPistol");
            else if (Index == MuzzleBrake) Art = TEXT("MuzzleBrake");
            else if (Index == VariableScope) Art = TEXT("VariableScope");
            else if (Index == IRSight) Art = TEXT("IRSight");
            else if (Index == ThermalSight) Art = TEXT("ThermalSight");
            else if (Mod.Slot == EWeaponMount::Laser || Index == IRIlluminator) Art = TEXT("PEQ15");
            Mount(Mod.Dimensions, Mod.AlongWeapon, Mod.Height, Art);
            const bool bLaser = Mod.Slot == EWeaponMount::Laser;
            if (bPreview || !IsLocallyControlled() || (!bLaser && Mod.Slot != EWeaponMount::Light)) continue;
            const bool bInfrared = Index == IRLaser || Index == IRIlluminator;
            if (bInfrared && !WornGear.ContainsByPredicate([](int32 Gear) {
                return Gear >= 0 && Gear < UE_ARRAY_COUNT(SurvivalGear) &&
                    (FCString::Strcmp(SurvivalGear[Gear].Id, TEXT("nvg_phosphor")) == 0 ||
                     FCString::Strcmp(SurvivalGear[Gear].Id, TEXT("quad_night_vision_goggle")) == 0 ||
                     FCString::Strcmp(SurvivalGear[Gear].Id, TEXT("tactical_modern_helmet_w_nvgs")) == 0);
            })) continue;
            auto* Light = NewObject<USpotLightComponent>(this);
            Light->SetupAttachment(FirstPersonCamera);
            Light->SetRelativeLocation(FVector(50, 3, -4));
            Light->SetCastShadows(false);
            Light->SetAttenuationRadius(bLaser ? 3500.f : 1800.f);
            Light->SetInnerConeAngle(bLaser ? .2f : 10.f);
            Light->SetOuterConeAngle(bLaser ? .4f : 24.f);
            Light->SetIntensity(bLaser ? 12000.f : 6000.f);
            Light->SetLightColor(Index == RedLaser ? FLinearColor::Red :
                (bLaser || bInfrared) ? FLinearColor::Green : FLinearColor::White);
            Light->RegisterComponent();
            AttachmentLights.Add(Light);
        }
    if (bRifle)
        for (int32 Slot=0; Slot<FSurvivalWeaponAssembly::PartCount; ++Slot)
        {
            const int32 Quality = GetWeaponAssembly(ESurvivalItem::Rifle).GetPartQuality(Slot);
            if (Quality > 0 && Quality != FSurvivalWeaponAssembly::FactoryQuality)
                Mount(FVector(3 + Quality * .0055, 5, 5), -.72 + .43*Slot, .25);
        }
}
