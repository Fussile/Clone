#include "CrossroadsWorldBake.h"

#include "Editor.h"
#include "Engine/World.h"
#include "Landscape.h"
#include "LandscapeSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"
#include "WorldPartition/WorldPartition.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, SurvivalWorldEditor)

bool UCrossroadsWorldBake::ImportTerrain(const FString& HeightFile, int32 Resolution, FVector Scale,
    UMaterialInterface* GroundMaterial)
{
    constexpr int32 SectionsPerComponent = 2;
    constexpr int32 SectionQuads = 63;
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || Resolution < 127 || Resolution > 4033
        || (Resolution - 1) % (SectionsPerComponent * SectionQuads) != 0
        || Scale.GetMin() <= 0.0)
    {
        return false;
    }

    TArray<uint8> Bytes;
    const int32 Samples = Resolution * Resolution;
    if (!FFileHelper::LoadFileToArray(Bytes, *HeightFile) || Bytes.Num() != Samples * sizeof(uint16))
    {
        return false;
    }

    TArray<uint16> Heights;
    Heights.SetNumUninitialized(Samples);
    for (int32 Index = 0; Index < Samples; ++Index)
    {
        Heights[Index] = Bytes[Index * 2] | static_cast<uint16>(Bytes[Index * 2 + 1]) << 8;
    }

    UWorldPartition* Partition = World->GetWorldPartition();
    if (Partition)
    {
        // Blank partitioned maps can inherit disabled streaming from editor defaults.
        if (FBoolProperty* Streaming = FindFProperty<FBoolProperty>(Partition->GetClass(), TEXT("bEnableStreaming")))
        {
            Partition->Modify();
            Streaming->SetPropertyValue_InContainer(Partition, true);
        }
    }

    ALandscape* Terrain = World->SpawnActor<ALandscape>();
    if (!Terrain)
    {
        return false;
    }

    const int32 Quads = Resolution - 1;
    Terrain->SetActorLocation(FVector(-Quads * Scale.X / 2, -Quads * Scale.Y / 2, 0.0));
    Terrain->SetActorScale3D(Scale);
    Terrain->SetActorLabel(TEXT("Crossroads terrain"));
    Terrain->Tags.Add(TEXT("StructuralGround"));
    Terrain->LandscapeMaterial = GroundMaterial;

    TMap<FGuid, TArray<uint16>> HeightLayers;
    HeightLayers.Add(FGuid(), MoveTemp(Heights));
    TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayers;
    MaterialLayers.Add(FGuid());
    Terrain->Import(FGuid::NewGuid(), 0, 0, Quads, Quads, SectionsPerComponent, SectionQuads,
        HeightLayers, nullptr, MaterialLayers, ELandscapeImportAlphamapType::Additive, {});
    Terrain->RegisterAllComponents();
    Terrain->PostEditChange();

    if (Partition)
    {
        if (ULandscapeSubsystem* Landscapes = World->GetSubsystem<ULandscapeSubsystem>())
        {
            if (ULandscapeInfo* Info = Terrain->GetLandscapeInfo())
            {
                Landscapes->ChangeGridSize(Info, 2);
            }
        }
    }
    Terrain->MarkPackageDirty();
    return true;
}
