#include "SurvivalSidearm.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

ASurvivalSidearm::ASurvivalSidearm()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    ImportedVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ImportedVisual"));
    ImportedVisual->SetupAttachment(GetRootComponent());
    ImportedVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ImportedVisual->SetCastShadow(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!Cube.Succeeded())
    {
        return;
    }

    // Original blockout prop in camera space (centimeters).
    const auto Part = [&](const TCHAR* PartName, const FVector& Location, const FVector& Size, double Pitch)
    {
        UStaticMeshComponent* Shape = CreateDefaultSubobject<UStaticMeshComponent>(FName(PartName));
        Shape->SetupAttachment(GetRootComponent());
        Shape->SetStaticMesh(Cube.Object);
        Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Shape->SetCastShadow(false);
        Shape->SetRelativeLocation(Location);
        Shape->SetRelativeScale3D(Size / 100.0);
        Shape->SetRelativeRotation(FRotator(Pitch, 0, 0));
    };
    Part(TEXT("Slide"), FVector(6, 0, 5), FVector(24, 4, 4), 0);
    Part(TEXT("Frame"), FVector(3, 0, 2), FVector(20, 5, 3), 0);
    Part(TEXT("Grip"), FVector(-4, 0, -4), FVector(6, 4.8, 12), 16);
    Part(TEXT("Sight"), FVector(14, 0, 7.5), FVector(2, 1, 1), 0);
}

void ASurvivalSidearm::BeginPlay()
{
    Super::BeginPlay();
    ShotSound = LoadObject<USoundBase>(nullptr,
        TEXT("/Game/Audio/Weapons/SW_SidearmShot.SW_SidearmShot"), nullptr, LOAD_NoWarn);
    TInlineComponentArray<UStaticMeshComponent*> Parts(this);
    UStaticMesh* SidearmAsset = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Game/Art/Crossroads/Building/SM_CrossroadsSidearm.SM_CrossroadsSidearm"), nullptr, LOAD_NoWarn);
    if (SidearmAsset)
    {
        const FBoxSphereBounds Bounds = SidearmAsset->GetBounds();
        const double Length = 2.0 * FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y);
        if (Length > UE_SMALL_NUMBER)
        {
            const double Scale = 24.0 / Length;
            const FRotator Rotation(0.0, Bounds.BoxExtent.Y > Bounds.BoxExtent.X ? -90.0 : 0.0, 0.0);
            ImportedVisual->SetStaticMesh(SidearmAsset);
            ImportedVisual->SetRelativeScale3D(FVector(Scale));
            ImportedVisual->SetRelativeRotation(Rotation);
            ImportedVisual->SetRelativeLocation(FVector(6, 0, 1) - Rotation.RotateVector(Bounds.Origin * Scale));
            for (UStaticMeshComponent* Part : Parts)
            {
                if (Part != ImportedVisual.Get())
                {
                    Part->SetVisibility(false);
                }
            }
            return;
        }
    }

    if (UMaterialInterface* Steel = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Art/Crossroads/Building/M_DefenseSteel.M_DefenseSteel")))
    {
        for (UStaticMeshComponent* Part : Parts)
        {
            Part->SetMaterial(0, Steel);
        }
    }
}

void ASurvivalSidearm::PlayShotSound()
{
    if (ShotSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, ShotSound.Get(), GetActorLocation());
    }
}
