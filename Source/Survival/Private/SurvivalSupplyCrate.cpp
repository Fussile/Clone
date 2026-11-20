#include "SurvivalSupplyCrate.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
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

int32 ASurvivalSupplyCrate::TakeSupplies()
{
    if (!HasSupplies())
    {
        return 0;
    }
    bSearched = true;
    return SupplyAmount;
}
