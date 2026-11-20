#pragma once

#include "GameFramework/Actor.h"
#include "SurvivalBuilding.h"
#include "SurvivalSupplyCrate.generated.h"

class UStaticMeshComponent;

UCLASS()
class ASurvivalSupplyCrate : public AActor
{
    GENERATED_BODY()

public:
    ASurvivalSupplyCrate();

    FSurvivalMaterials TakeMaterials();
    bool HasMaterials() const { return !bSearched && (TimberAmount > 0 || StoneAmount > 0); }

private:
    UPROPERTY(VisibleAnywhere, Category = "Scavenging")
    TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(EditAnywhere, Category = "Scavenging", meta = (ClampMin = "0"))
    int32 TimberAmount = 24;

    UPROPERTY(EditAnywhere, Category = "Scavenging", meta = (ClampMin = "0"))
    int32 StoneAmount = 12;

    bool bSearched = false;
};
