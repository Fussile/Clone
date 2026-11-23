#include "CrossroadsWorldBake.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Landscape.h"
#include "LandscapeSubsystem.h"
#include "Materials/Material.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionSetMaterialAttributes.h"
#include "MeshDescription.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"
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


bool UCrossroadsWorldBake::SetStaticMeshCollision(UStaticMesh* Mesh)
{
    if (!Mesh)
    {
        return false;
    }
    Mesh->CreateBodySetup();
    UBodySetup* Body = Mesh->GetBodySetup();
    if (Body->CollisionTraceFlag != CTF_UseComplexAsSimple || !Body->bDoubleSidedGeometry)
    {
        Mesh->Modify();
        Body->Modify();
        Body->CollisionTraceFlag = CTF_UseComplexAsSimple;
        Body->bDoubleSidedGeometry = true;
        Body->InvalidatePhysicsData();
        Body->CreatePhysicsMeshes();
        Mesh->MarkPackageDirty();
    }
    return true;
}

void UCrossroadsWorldBake::DisableMaterialEmission(UMaterial* Material)
{
    if (!Material)
    {
        return;
    }

    Material->Modify();
    UMaterialEditorOnlyData* Data = Material->GetEditorOnlyData();
    auto* Zero = NewObject<UMaterialExpressionConstant>(Material);
    Zero->R = 0.0f;
    Data->ExpressionCollection.AddExpression(Zero);
    if (Material->bUseMaterialAttributes && Data->MaterialAttributes.IsConnected())
    {
        auto* Attributes = NewObject<UMaterialExpressionSetMaterialAttributes>(Material);
        Attributes->AttributeSetTypes.Add(FMaterialAttributeDefinitionMap::GetID(MP_EmissiveColor));
        Attributes->Inputs.SetNum(2);
        Attributes->Inputs[0] = static_cast<const FExpressionInput&>(Data->MaterialAttributes);
        Attributes->Inputs[1].Connect(0, Zero);
        Data->ExpressionCollection.AddExpression(Attributes);
        Data->MaterialAttributes.Connect(0, Attributes);
    }
    else
    {
        Data->EmissiveColor.Connect(0, Zero);
    }
    Material->PostEditChange();
    Material->MarkPackageDirty();
}

UStaticMesh* UCrossroadsWorldBake::CreateRoadMesh(const FString& PackagePath,
    const TArray<FVector>& Positions, const TArray<FVector2D>& UVs, const TArray<int32>& Indices,
    UMaterialInterface* Material)
{
    if (Positions.Num() != UVs.Num() || Indices.IsEmpty() || Indices.Num() % 3 != 0)
    {
        return nullptr;
    }

    FMeshDescription Description;
    FStaticMeshAttributes Attributes(Description);
    Attributes.Register();
    auto VertexPositions = Attributes.GetVertexPositions();
    auto VertexUVs = Attributes.GetVertexInstanceUVs();
    auto Normals = Attributes.GetVertexInstanceNormals();
    auto Tangents = Attributes.GetVertexInstanceTangents();
    auto Signs = Attributes.GetVertexInstanceBinormalSigns();
    VertexUVs.SetNumChannels(1);
    TArray<FVertexID> Vertices;
    for (const FVector& Position : Positions)
    {
        const FVertexID Id = Description.CreateVertex();
        VertexPositions[Id] = FVector3f(Position);
        Vertices.Add(Id);
    }

    const FName SurfaceName(TEXT("Pavement"));
    const FPolygonGroupID Group = Description.CreatePolygonGroup();
    Attributes.GetPolygonGroupMaterialSlotNames()[Group] = SurfaceName;
    for (int32 Face = 0; Face < Indices.Num(); Face += 3)
    {
        const int32 A = Indices[Face], B = Indices[Face + 1], C = Indices[Face + 2];
        if (!Vertices.IsValidIndex(A) || !Vertices.IsValidIndex(B) || !Vertices.IsValidIndex(C))
        {
            return nullptr;
        }

        const FVector Edge = Positions[B] - Positions[A];
        const FVector Normal = FVector::CrossProduct(Positions[C] - Positions[A], Edge).GetSafeNormal();
        if (Normal.IsNearlyZero())
        {
            continue;
        }

        TArray<FVertexInstanceID, TInlineAllocator<3>> Corners;
        for (const int32 Index : { A, B, C })
        {
            const FVertexInstanceID Id = Description.CreateVertexInstance(Vertices[Index]);
            Corners.Add(Id);
            Normals[Id] = FVector3f(Normal);
            Tangents[Id] = FVector3f(Edge.GetSafeNormal());
            Signs[Id] = 1.0f;
            VertexUVs.Set(Id, 0, FVector2f(UVs[Index]));
        }
        Description.CreateTriangle(Group, Corners);
    }

    UPackage* Package = CreatePackage(*PackagePath);
    const FName AssetName(*FPackageName::GetLongPackageAssetName(PackagePath));
    UStaticMesh* Mesh = NewObject<UStaticMesh>(Package, AssetName, RF_Public | RF_Standalone);
    Mesh->GetStaticMaterials().Emplace(Material, SurfaceName, SurfaceName);
    UStaticMesh::FBuildMeshDescriptionsParams Params;
    Params.bFastBuild = true;
    Params.bCommitMeshDescription = true;
    Params.bAllowCpuAccess = true;
    Params.bBuildSimpleCollision = false;
    TArray<const FMeshDescription*> Descriptions{ &Description };
    if (!Mesh->BuildFromMeshDescriptions(Descriptions, Params) || !SetStaticMeshCollision(Mesh))
    {
        return nullptr;
    }

    FAssetRegistryModule::AssetCreated(Mesh);
    Mesh->MarkPackageDirty();
    return Mesh;
}
