#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "CrossroadsWorldBake.generated.h"

class AActor;
class UMaterialInterface;
class UStaticMesh;

UCLASS()
class UCrossroadsWorldBake : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Crossroads World Bake")
    static bool ImportTerrain(const FString& HeightFile, int32 Resolution, FVector Scale, UMaterialInterface* GroundMaterial);

    UFUNCTION(BlueprintCallable, Category = "Crossroads World Bake")
    static bool SetStaticMeshCollision(UStaticMesh* Mesh);

    UFUNCTION(BlueprintCallable, Category = "Crossroads World Bake")
    static TArray<UMaterialInterface*> GetLandmarkMaterials(AActor* Source);

    UFUNCTION(BlueprintCallable, Category = "Crossroads World Bake")
    static bool CopyLandmarkSkin(AActor* Target, AActor* Source, FVector Dimensions,
        double EntryWidth, double EntryHeight,
        const TMap<UMaterialInterface*, UMaterialInterface*>& Finishes, UMaterialInterface* DefaultFinish);

    UFUNCTION(BlueprintCallable, Category = "Crossroads World Bake")
    static UStaticMesh* CreateRoadMesh(const FString& PackagePath, const TArray<FVector>& Positions,
        const TArray<FVector2D>& UVs, const TArray<int32>& Indices, UMaterialInterface* Material);
};
