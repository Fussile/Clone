#include "SurvivalStructurePrefab.h"

#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "SurvivalGameMode.h"

namespace
{
void SetAuthoredWall(FSurvivalBuildPlacement& Placement, UStaticMesh& Mesh, UMaterialInterface* Material)
{
    // The sliced wall has a floor pivot and its visible face is offset outward along its thin axis.
    // Keep the collider and grid in prefab space even if the importer changes the mesh's forward axis.
    const FBox MeshBox = Mesh.GetBoundingBox();
    const FVector Extent = MeshBox.GetExtent();
    const FVector Center = MeshBox.GetCenter();
    const bool bDepthAlongX = Extent.X < Extent.Y;
    const double Yaw = bDepthAlongX ? (Center.X > 0 ? 90.0 : -90.0) : (Center.Y > 0 ? 0.0 : 180.0);
    const FQuat MeshRotation = FRotator(0, Yaw, 0).Quaternion();
    const FBox Oriented = MeshBox.TransformBy(FTransform(MeshRotation));
    constexpr double Cell = FSurvivalBuildPlacement::GridSize;
    constexpr double Half = Cell / 2.0;
    // Keep adjacent faces in contact for support, including cells where the art is a single plane.
    Placement.AuthoredWallBounds = FBox(
        FVector(-Half, FMath::Clamp(Oriented.Min.Y - 1.0, -Half, 22.0), 0),
        FVector(Half, FMath::Clamp(Oriented.Max.Y + 1.0, 28.0, Half), Cell));
    Placement.AuthoredWallMesh = &Mesh;
    Placement.AuthoredWallMaterial = Material;
    Placement.AuthoredWallMeshRotation = MeshRotation;
}
}

ASurvivalStructurePrefab::ASurvivalStructurePrefab()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("PrefabOrigin")));
}

void ASurvivalStructurePrefab::BeginPlay()
{
    Super::BeginPlay();
    ASurvivalGameMode* GameMode = GetWorld()->GetAuthGameMode<ASurvivalGameMode>();
    if (!GameMode || !GameMode->BeginAuthoredPrefab(PrefabId))
    {
        return;
    }

    // Author each descriptor from footing to roof so every part has its intended support on spawn.
    for (const FSurvivalPrefabPart& Part : Parts)
    {
        if (!GetSurvivalBuildPieces().IsValidIndex(Part.PieceIndex))
        {
            continue;
        }
        FSurvivalBuildPlacement Placement;
        Placement.Base = GetActorTransform().TransformPositionNoScale(Part.Offset);
        Placement.Rotation = GetActorQuat() * FRotator(0, Part.QuarterTurns * 90.0, 0).Quaternion();
        Placement.PieceIndex = Part.PieceIndex;
        if (Part.WallMesh)
        {
            SetAuthoredWall(Placement, *Part.WallMesh, WallMaterial.Get());
        }
        if (!GameMode->AddAuthoredPiece(Placement))
        {
            UE_LOG(LogTemp, Warning, TEXT("%s has an unsupported authored piece at %s."),
                *GetName(), *Placement.Base.ToString());
        }
    }
}
