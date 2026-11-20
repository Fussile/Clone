#include "SurvivalCharacter.h"
#include "SurvivalSupplyCrate.h"

#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
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
