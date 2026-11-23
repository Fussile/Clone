#pragma once

#include "GameFramework/Actor.h"
#include "SurvivalFacade.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

UCLASS()
class ASurvivalFacade : public AActor
{
    GENERATED_BODY()

public:
    ASurvivalFacade();

    UFUNCTION(BlueprintCallable, Category = "City")
    void FitPanels(UStaticMesh* PanelMesh, double WidthCm, double DepthCm, double HeightCm,
        double GroundHeightCm, double EntranceWidthCm, const TArray<UMaterialInterface*>& Materials);

private:
    UPROPERTY(VisibleAnywhere, Category = "City")
    TObjectPtr<UInstancedStaticMeshComponent> Panels;
};
