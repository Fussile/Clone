#include "SurvivalSidearm.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ASurvivalSidearm::ASurvivalSidearm()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
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
    if (UMaterialInterface* Steel = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Art/Crossroads/Building/M_DefenseSteel.M_DefenseSteel")))
    {
        TInlineComponentArray<UStaticMeshComponent*> Parts(this);
        for (UStaticMeshComponent* Part : Parts)
        {
            Part->SetMaterial(0, Steel);
        }
    }
}
