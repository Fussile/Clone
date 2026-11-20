#include "SurvivalSupplyCrate.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ASurvivalSupplyCrate::ASurvivalSupplyCrate()
{
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CrateMesh"));
    SetRootComponent(Mesh);
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cube.Succeeded())
    {
        Mesh->SetStaticMesh(Cube.Object);
    }
}

void ASurvivalSupplyCrate::BeginPlay()
{
    Super::BeginPlay();
    if (bContainsSidearm)
    {
        if (UMaterialInterface* Steel = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Art/Crossroads/Building/M_DefenseSteel.M_DefenseSteel")))
        {
            Mesh->SetMaterial(0, Steel);
        }
    }
}

FSurvivalMaterials ASurvivalSupplyCrate::TakeMaterials()
{
    if (!HasMaterials())
    {
        return {};
    }
    bSearched = true;
    return {FMath::Max(0, TimberAmount), FMath::Max(0, StoneAmount)};
}

bool ASurvivalSupplyCrate::TakeSidearm()
{
    if (!HasSidearm())
    {
        return false;
    }
    bSearched = true;
    return true;
}
