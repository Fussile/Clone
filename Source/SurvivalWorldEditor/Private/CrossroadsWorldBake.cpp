#include "CrossroadsWorldBake.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Landscape.h"
#include "LandscapeSubsystem.h"
#include "Materials/MaterialInterface.h"
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

namespace
{
bool IsVisibleLandmarkMesh(const UStaticMeshComponent* Component)
{
    return Component->GetStaticMesh() && Component->GetVisibleFlag() && !Component->bHiddenInGame;
}
}

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

TArray<UMaterialInterface*> UCrossroadsWorldBake::GetLandmarkMaterials(AActor* Source)
{
    if (!Source)
    {
        return {};
    }

    TSet<UMaterialInterface*> Materials;
    TArray<UStaticMeshComponent*> Components;
    Source->GetComponents(Components, true);
    for (const UStaticMeshComponent* Component : Components)
    {
        if (!IsVisibleLandmarkMesh(Component))
        {
            continue;
        }
        for (int32 Slot = 0; Slot < Component->GetNumMaterials(); ++Slot)
        {
            if (UMaterialInterface* Material = Component->GetMaterial(Slot))
            {
                Materials.Add(Material);
            }
        }
    }
    return Materials.Array();
}

bool UCrossroadsWorldBake::CopyLandmarkSkin(AActor* Target, AActor* Source, FVector Dimensions,
    double EntryWidth, double EntryHeight,
    const TMap<UMaterialInterface*, UMaterialInterface*>& Finishes, UMaterialInterface* DefaultFinish)
{
    if (!Target || !Target->GetRootComponent() || !Source)
    {
        return false;
    }

    const FName SkinTag(TEXT("CrossroadsLandmarkSkin"));
    struct FMeshGroup
    {
        UStaticMeshComponent* Component;
        TArray<FTransform> Transforms;
    };
    TArray<FMeshGroup> Groups;
    TArray<UStaticMeshComponent*> SourceComponents;
    Source->GetComponents(SourceComponents, true);
    const FTransform Origin = Source->GetActorTransform();
    FBox Envelope(ForceInit);
    for (UStaticMeshComponent* Component : SourceComponents)
    {
        if (!IsVisibleLandmarkMesh(Component))
        {
            continue;
        }
        UStaticMesh* Mesh = Component->GetStaticMesh();
        FMeshGroup& Group = Groups.AddDefaulted_GetRef();
        Group.Component = Component;
        if (auto* Instanced = Cast<UInstancedStaticMeshComponent>(Component))
        {
            for (int32 Index = 0; Index < Instanced->GetInstanceCount(); ++Index)
            {
                FTransform Instance;
                if (Instanced->GetInstanceTransform(Index, Instance, true))
                {
                    Group.Transforms.Add(Instance.GetRelativeTransform(Origin));
                }
            }
        }
        else
        {
            Group.Transforms.Add(Component->GetComponentTransform().GetRelativeTransform(Origin));
        }
        for (const FTransform& Instance : Group.Transforms)
        {
            Envelope += Mesh->GetBoundingBox().TransformBy(Instance);
        }
    }
    if (!Envelope.IsValid || Envelope.GetSize().GetMin() <= UE_SMALL_NUMBER)
    {
        return false;
    }

    const FVector Size = Envelope.GetSize();
    const FVector Scale(Dimensions.X / Size.X, Dimensions.Y / Size.Y, Dimensions.Z / Size.Z);
    const FVector Base(Envelope.GetCenter().X, Envelope.GetCenter().Y, Envelope.Min.Z);
    const double HalfEntry = (EntryWidth * 0.5 + 18.0) / Scale.X;
    const FBox Opening(FVector(Base.X - HalfEntry, Base.Y + Size.Y * 0.05, Base.Z + 14.0 / Scale.Z),
        FVector(Base.X + HalfEntry, Envelope.Max.Y + 1.0, Base.Z + EntryHeight / Scale.Z));
    int32 VisibleInstances = 0;
    int32 RemovedInstances = 0;
    for (FMeshGroup& Group : Groups)
    {
        const FBox Bounds = Group.Component->GetStaticMesh()->GetBoundingBox();
        if (EntryWidth > 0.0)
        {
            RemovedInstances += Group.Transforms.RemoveAll([&](const FTransform& Instance)
            {
                return Bounds.TransformBy(Instance).Intersect(Opening);
            });
        }
        for (FTransform& Instance : Group.Transforms)
        {
            Instance.AddToTranslation(-Base);
        }
        VisibleInstances += Group.Transforms.Num();
    }
    if (VisibleInstances == 0)
    {
        return false;
    }

    TArray<UInstancedStaticMeshComponent*> OldComponents;
    Target->GetComponents(OldComponents);
    Target->Modify();
    for (UActorComponent* Component : OldComponents)
    {
        if (Component->ComponentHasTag(SkinTag))
        {
            Component->Modify();
            Target->RemoveInstanceComponent(Component);
            Component->DestroyComponent();
        }
    }

    Target->SetActorScale3D(Scale);
    for (const FMeshGroup& Group : Groups)
    {
        if (Group.Transforms.IsEmpty())
        {
            continue;
        }

        auto* Skin = NewObject<UInstancedStaticMeshComponent>(Target, NAME_None, RF_Transactional);
        Target->AddInstanceComponent(Skin);
        Skin->ComponentTags.Add(SkinTag);
        Skin->SetupAttachment(Target->GetRootComponent());
        Skin->SetMobility(EComponentMobility::Static);
        Skin->SetStaticMesh(Group.Component->GetStaticMesh());
        Skin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Skin->SetCanEverAffectNavigation(false);
        for (int32 Slot = 0; Slot < Group.Component->GetNumMaterials(); ++Slot)
        {
            UMaterialInterface* const* Finish = Finishes.Find(Group.Component->GetMaterial(Slot));
            Skin->SetMaterial(Slot, Finish ? *Finish : DefaultFinish);
        }
        Skin->AddInstances(Group.Transforms, false, false, false);
        Skin->RegisterComponent();
    }
    Target->MarkPackageDirty();
    UE_LOG(LogTemp, Display, TEXT("Crossroads City Sample %s: copied %d mesh instances, removed %d at the entrance."),
        *Source->GetClass()->GetName(), VisibleInstances, RemovedInstances);
    return true;
}
