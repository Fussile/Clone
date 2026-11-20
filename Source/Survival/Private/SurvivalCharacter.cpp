#include "SurvivalCharacter.h"
#include "SurvivalGameMode.h"
#include "SurvivalSupplyCrate.h"

#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"

ASurvivalCharacter::ASurvivalCharacter()
{
    bReplicates = true;
    SetReplicateMovement(true);
    bUseControllerRotationYaw = true;

    FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
    FirstPersonCamera->SetRelativeLocation(FVector(0.0, 0.0, BaseEyeHeight));
    FirstPersonCamera->bUsePawnControlRotation = true;
}

void ASurvivalCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &ASurvivalCharacter::MoveForward);
    PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &ASurvivalCharacter::MoveRight);
    PlayerInputComponent->BindAxis(TEXT("Turn"), this, &APawn::AddControllerYawInput);
    PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &APawn::AddControllerPitchInput);
    PlayerInputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &ACharacter::Jump);
    PlayerInputComponent->BindAction(TEXT("Jump"), IE_Released, this, &ACharacter::StopJumping);
    PlayerInputComponent->BindAction(TEXT("Scavenge"), IE_Pressed, this, &ASurvivalCharacter::Scavenge);
    PlayerInputComponent->BindAction(TEXT("ToggleBuildMode"), IE_Pressed, this, &ASurvivalCharacter::ToggleBuildMode);
    PlayerInputComponent->BindAction(TEXT("RotateWall"), IE_Pressed, this, &ASurvivalCharacter::RotateWall);
    PlayerInputComponent->BindAction(TEXT("PlaceWall"), IE_Pressed, this, &ASurvivalCharacter::PlaceWall);
}

void ASurvivalCharacter::MoveForward(float Value)
{
    AddMovementInput(GetActorForwardVector(), Value);
}

void ASurvivalCharacter::MoveRight(float Value)
{
    AddMovementInput(GetActorRightVector(), Value);
}

ASurvivalSupplyCrate* ASurvivalCharacter::FindSupplyCrate() const
{
    constexpr float ReachCm = 260.0f;
    const FVector Start = FirstPersonCamera->GetComponentLocation();
    const FVector End = Start + FirstPersonCamera->GetForwardVector() * ReachCm;
    const FCollisionQueryParams Query(SCENE_QUERY_STAT(Scavenge), false, this);
    FHitResult Hit;
    if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Query))
    {
        return Cast<ASurvivalSupplyCrate>(Hit.GetActor());
    }
    return nullptr;
}

void ASurvivalCharacter::Scavenge()
{
    if (ASurvivalSupplyCrate* Crate = FindSupplyCrate())
    {
        SupplyCount += Crate->TakeSupplies();
    }
}

void ASurvivalCharacter::ToggleBuildMode()
{
    bBuildMode = !bBuildMode;
}

void ASurvivalCharacter::RotateWall()
{
    if (bBuildMode)
    {
        bWallRotated = !bWallRotated;
    }
}

TOptional<FSurvivalWallPlacement> ASurvivalCharacter::FindWallPlacement() const
{
    constexpr double Reach = 500.0;
    constexpr double SupportTolerance = 8.0;
    const FVector View = FirstPersonCamera->GetComponentLocation();
    const FVector End = View + FirstPersonCamera->GetForwardVector() * Reach;
    const UWorld* World = GetWorld();
    const FCollisionQueryParams TraceQuery(SCENE_QUERY_STAT(WallPlacement), false, this);
    FHitResult AimHit;
    if (!World->LineTraceSingleByChannel(AimHit, View, End, ECC_Visibility, TraceQuery)
        || AimHit.ImpactNormal.Z < 0.95)
    {
        return {};
    }

    const FVector Snapped(
        FMath::GridSnap(AimHit.ImpactPoint.X, FSurvivalWallPlacement::GridSize),
        FMath::GridSnap(AimHit.ImpactPoint.Y, FSurvivalWallPlacement::GridSize),
        AimHit.ImpactPoint.Z);
    FSurvivalWallPlacement Placement;
    Placement.Center = Snapped + FVector(0, 0, FSurvivalWallPlacement::HalfExtent.Z);
    Placement.Rotation = bWallRotated ? FRotator(0, 90, 0).Quaternion() : FQuat::Identity;

    // Snapping can move a position to the far side of an obstacle.
    const FVector SightTarget = Snapped + FVector(0, 0, SupportTolerance + 2.0);
    if (FVector::DistSquared(View, Snapped) > FMath::Square(Reach)
        || World->LineTraceTestByChannel(View, SightTarget, ECC_Visibility, TraceQuery))
    {
        return TOptional<FSurvivalWallPlacement>(Placement);
    }

    const FVector Half = FSurvivalWallPlacement::HalfExtent;
    for (int32 XSide : {-1, 1})
    {
        for (int32 YSide : {-1, 1})
        {
            const FVector Foot = Snapped + Placement.Rotation.RotateVector(
                FVector(XSide * (Half.X - 2.0), YSide * (Half.Y - 2.0), 0.0));
            const FVector Probe(0, 0, SupportTolerance);
            FHitResult SupportHit;
            if (!World->LineTraceSingleByChannel(SupportHit, Foot + Probe, Foot - Probe, ECC_Visibility, TraceQuery)
                || SupportHit.ImpactNormal.Z < 0.95 || !SupportHit.GetComponent()
                || SupportHit.GetComponent()->Mobility != EComponentMobility::Static)
            {
                return TOptional<FSurvivalWallPlacement>(Placement);
            }
        }
    }

    // Include the player in the overlap check. Inset the faces so neighboring walls can touch.
    Placement.bClear = !World->OverlapBlockingTestByChannel(Placement.Center, Placement.Rotation, ECC_Pawn,
        FCollisionShape::MakeBox(Half - FVector(0.5)), FCollisionQueryParams::DefaultQueryParam,
        FCollisionResponseParams::DefaultResponseParam);
    return TOptional<FSurvivalWallPlacement>(Placement);
}

void ASurvivalCharacter::PlaceWall()
{
    if (!bBuildMode || SupplyCount <= 0)
    {
        return;
    }

    const TOptional<FSurvivalWallPlacement> Placement = FindWallPlacement();
    ASurvivalGameMode* GameMode = GetWorld()->GetAuthGameMode<ASurvivalGameMode>();
    if (Placement.IsSet() && Placement->bClear && GameMode && GameMode->AddWall(*Placement))
    {
        --SupplyCount;
    }
}
