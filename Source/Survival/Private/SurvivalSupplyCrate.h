#pragma once

#include "GameFramework/Actor.h"
#include "SurvivalBuilding.h"
#include "SurvivalVitals.h"
#include "SurvivalSupplyCrate.generated.h"

class UStaticMeshComponent;

struct FSurvivalLoot
{
    FSurvivalMaterials Materials;
    FSurvivalItems Items;
    bool bSidearm = false;
    bool bRifle = false;
};

UCLASS()
class ASurvivalSupplyCrate : public AActor
{
    GENERATED_BODY()

public:
    ASurvivalSupplyCrate();

    FSurvivalLoot TakeLoot();
    bool HasSidearm() const { return !bSearched && bContainsSidearm; }
    bool HasRifle() const { return !bSearched && bContainsRifle; }
    bool HasSupplies() const
    {
        return !bSearched && (TimberAmount > 0 || StoneAmount > 0 || FoodAmount > 0
            || WaterAmount > 0 || ClothAmount > 0 || bContainsRifle || BuilderGuideAmount > 0
            || ScavengerNotesAmount > 0 || DressingSchematicAmount > 0
            || (RiflePartSlot >= 0 && RiflePartSlot < FSurvivalRifle::PartCount));
    }

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere, Category = "Scavenging")
    TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(EditAnywhere, Category = "Scavenging", meta = (ClampMin = "0"))
    int32 TimberAmount = 24;

    UPROPERTY(EditAnywhere, Category = "Scavenging", meta = (ClampMin = "0"))
    int32 StoneAmount = 12;

    UPROPERTY(EditAnywhere, Category = "Scavenging")
    bool bContainsSidearm = false;

    UPROPERTY(EditAnywhere, Category = "Scavenging", meta = (ClampMin = "0"))
    int32 FoodAmount = 0;

    UPROPERTY(EditAnywhere, Category = "Scavenging", meta = (ClampMin = "0"))
    int32 WaterAmount = 0;

    UPROPERTY(EditAnywhere, Category = "Scavenging", meta = (ClampMin = "0"))
    int32 ClothAmount = 0;

    UPROPERTY(EditAnywhere, Category = "Scavenging")
    bool bContainsRifle = false;

    UPROPERTY(EditAnywhere, Category = "Scavenging", meta = (ClampMin = "0"))
    int32 BuilderGuideAmount = 0;

    UPROPERTY(EditAnywhere, Category = "Scavenging", meta = (ClampMin = "0"))
    int32 ScavengerNotesAmount = 0;

    UPROPERTY(EditAnywhere, Category = "Scavenging", meta = (ClampMin = "0"))
    int32 DressingSchematicAmount = 0;

    UPROPERTY(EditAnywhere, Category = "Scavenging", meta = (ClampMin = "-1", ClampMax = "3"))
    int32 RiflePartSlot = -1;

    bool bSearched = false;
};
