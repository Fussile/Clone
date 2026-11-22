#pragma once

#include "GameFramework/Actor.h"
#include "SurvivalBuilding.h"
#include "SurvivalVitals.h"
#include "SurvivalSupplyCrate.generated.h"

class UStaticMeshComponent;

UENUM()
enum class ESurvivalLootProfile : uint8
{
    Authored,
    General,
    Food,
    Building,
    Medical,
    Security,
    Office
};

USTRUCT()
struct FSurvivalLoot
{
    GENERATED_BODY()

    UPROPERTY()
    FSurvivalMaterials Materials;

    UPROPERTY()
    FSurvivalItems Items;

    UPROPERTY()
    bool bSidearm = false;

    UPROPERTY()
    bool bRifle = false;

    bool IsEmpty() const;
};

struct FSurvivalScavengeState
{
    float Health = 0.0f;
    bool bSearched = false;
};
UCLASS()
class ASurvivalSupplyCrate : public AActor
{
    GENERATED_BODY()

public:
    ASurvivalSupplyCrate();

    FSurvivalLoot TakeLoot();
    virtual float TakeDamage(float Damage, const FDamageEvent& Event, AController* InstigatorController,
        AActor* DamageCauser) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    bool HasSidearm() const { return bLootAvailable && bSidearmAvailable; }
    bool HasRifle() const { return bLootAvailable && bRifleAvailable; }
    bool HasSupplies() const { return bLootAvailable; }

    FName GetLootId() const;
    void SetLootCycle(int32 Seed, const FSurvivalScavengeState* Previous);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

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

    UPROPERTY(EditAnywhere, Category = "Scavenging")
    ESurvivalLootProfile LootProfile = ESurvivalLootProfile::Authored;

    UPROPERTY(EditAnywhere, Category = "Scavenging", meta = (ClampMin = "0", ClampMax = "100"))
    int32 SpawnChancePercent = 100;

    UPROPERTY(EditAnywhere, Category = "Scavenging", meta = (ClampMin = "1"))
    float MaxHealth = 60.0f;

    UPROPERTY(ReplicatedUsing = OnRep_WorldPresence)
    bool bWorldPresent = true;

    UPROPERTY(Replicated)
    bool bLootAvailable = false;

    UPROPERTY(Replicated)
    bool bSidearmAvailable = false;

    UPROPERTY(Replicated)
    bool bRifleAvailable = false;

    UFUNCTION()
    void OnRep_WorldPresence();

    FSurvivalLoot RollLoot(FRandomStream& Random) const;
    void RememberState();

    FSurvivalLoot Contents;
    FSurvivalScavengeState State;
};
