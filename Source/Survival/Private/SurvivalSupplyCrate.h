#pragma once

#include "GameFramework/Actor.h"
#include "SurvivalSupplyCrate.generated.h"

class UStaticMeshComponent;

UCLASS()
class ASurvivalSupplyCrate : public AActor
{
    GENERATED_BODY()

public:
    ASurvivalSupplyCrate();

    int32 TakeSupplies();
    bool HasSupplies() const { return !bSearched && SupplyAmount > 0; }

private:
    UPROPERTY(VisibleAnywhere, Category = "Scavenging")
    TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(EditAnywhere, Category = "Scavenging", meta = (ClampMin = "1"))
    int32 SupplyAmount = 3;

    bool bSearched = false;
};
