#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "CrossroadsWorldBake.generated.h"

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
};
