#include "SurvivalWallChunk.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "SurvivalWallPlacement.h"
#include "UObject/ConstructorHelpers.h"

ASurvivalWallChunk::ASurvivalWallChunk()
{
    Walls = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Walls"));
    SetRootComponent(Walls);
    Walls->SetMobility(EComponentMobility::Movable);
    Walls->SetCollisionProfileName(TEXT("BlockAllDynamic"));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cube.Succeeded())
    {
        Walls->SetStaticMesh(Cube.Object);
    }
}

bool ASurvivalWallChunk::AddWall(const FSurvivalWallPlacement& Placement)
{
    constexpr double CubeHalfExtent = 50.0;
    const FTransform Transform(
        Placement.Rotation, Placement.Center, FSurvivalWallPlacement::HalfExtent / CubeHalfExtent);
    return Walls->GetStaticMesh() && Walls->AddInstance(Transform, /*bWorldSpace=*/true) != INDEX_NONE;
}
