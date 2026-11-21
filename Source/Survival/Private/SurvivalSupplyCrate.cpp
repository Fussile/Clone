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
    if (bContainsSidearm || bContainsRifle)
    {
        if (UMaterialInterface* Steel = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Art/Crossroads/Building/M_DefenseSteel.M_DefenseSteel")))
        {
            Mesh->SetMaterial(0, Steel);
        }
    }
}

FSurvivalLoot ASurvivalSupplyCrate::TakeLoot()
{
    if (!HasSupplies() && !HasSidearm())
    {
        return {};
    }
    bSearched = true;
    FSurvivalLoot Loot;
    Loot.Materials = {FMath::Max(0, TimberAmount), FMath::Max(0, StoneAmount)};
    Loot.Items.Food = FMath::Max(0, FoodAmount);
    Loot.Items.Water = FMath::Max(0, WaterAmount);
    Loot.Items.Cloth = FMath::Max(0, ClothAmount);
    Loot.Items.BuilderGuides = FMath::Max(0, BuilderGuideAmount);
    Loot.Items.ScavengerNotes = FMath::Max(0, ScavengerNotesAmount);
    Loot.Items.DressingSchematics = FMath::Max(0, DressingSchematicAmount);
    if (RiflePartSlot >= 0 && RiflePartSlot < FSurvivalRifle::PartCount)
    {
        Loot.Items.RifleParts[RiflePartSlot] = 1;
    }
    Loot.Items.RifleRounds = bContainsRifle ? 8 : 0;
    Loot.bSidearm = bContainsSidearm;
    Loot.bRifle = bContainsRifle;
    return Loot;
}
