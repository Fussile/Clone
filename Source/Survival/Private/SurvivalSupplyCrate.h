#pragma once

#include "GameFramework/Actor.h"
#include "Misc/Optional.h"
#include "SurvivalBuilding.h"
#include "SurvivalVitals.h"
#include "SurvivalSupplyCrate.generated.h"

class UStaticMeshComponent;
class ACharacter;

UENUM()
enum class ESurvivalLootProfile : uint8
{
    Authored,
    General,
    Food,
    Building,
    Medical,
    Security,
    Office,
    Vehicle,
    Mailbox,
    MilitaryArmory,
    Storage,
    Dropped UMETA(Hidden),
    PoliceArmory
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

    UPROPERTY()
    int32 GearIndex = INDEX_NONE;

    bool IsEmpty() const;
    int32 Count(ESurvivalItem Item) const;
    int32* StackCount(ESurvivalItem Item);

    int32 SidearmQuality = 0;
    int32 RifleQuality = 0;
};

USTRUCT()
struct FSurvivalLootSlot
{
    GENERATED_BODY()

    UPROPERTY()
    uint8 ItemId = static_cast<uint8>(ESurvivalItem::None);
    UPROPERTY()
    int32 Count = 0;
    UPROPERTY()
    int32 Quality = 0;
    UPROPERTY()
    int32 GearIndex = INDEX_NONE;

    UPROPERTY()
    int32 Firearm = INDEX_NONE;

    ESurvivalItem Item() const { return static_cast<ESurvivalItem>(ItemId); }
    bool IsEmpty() const { return Count <= 0; }
    FSurvivalPackCell ToPackCell() const { return {Item(), Count, Quality, GearIndex}; }
};

struct FSurvivalScavengeState
{
    float Health = 0.0f;
    bool bSearched = false;
    TArray<FSurvivalLootSlot> Slots;
};
UCLASS()
class ASurvivalSupplyCrate : public AActor
{
    GENERATED_BODY()

public:
    ASurvivalSupplyCrate();

    static constexpr int32 MaxLootSlots = 36;
    static constexpr int32 ChestSlots = 16;
    const TArray<FSurvivalLootSlot>& GetSlots() const { return State.Slots; }
    int32 GetLootRevision() const { return LootRevision; }
    int32 GetLootSeed() const { return LootSeed; }
    bool HasBeenSearched() const { return bWasSearched; }
    bool IsDroppedLoot() const { return bDropped; }
    FString GetLootName() const;
    float GetSearchDuration() const;
    bool FinishSearch(int32 GameStage);
    void MoveLoot(int32 From, int32 To, bool bSplit);
    TArray<FSurvivalLootSlot> TakeLoot(const TArray<int32>& Indices);
    static bool Drop(ACharacter& Owner, const FSurvivalLoot& Loot);
    virtual float TakeDamage(float Damage, const FDamageEvent& Event, AController* InstigatorController,
        AActor* DamageCauser) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
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

    UPROPERTY(EditAnywhere, Replicated, Category = "Scavenging")
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
    bool bWasSearched = false;

    UPROPERTY(EditAnywhere, Category = "Scavenging")
    FString LootName;

    int32 LootRevision = 0;
    int32 LootSeed = 0;

    UFUNCTION()
    void OnRep_WorldPresence();

    FSurvivalLoot GetAuthoredLoot() const;

    // Overrides can raise a site's reward floor and adjust its profile's regional scaling.
    UPROPERTY(EditAnywhere, Category = "Scavenging", meta = (ClampMin = "0", ClampMax = "8192"))
    int32 MinimumLootValue = 0;
    UPROPERTY(EditAnywhere, Category = "Scavenging", meta = (ClampMin = "0.25", ClampMax = "4.0"))
    float LootDifficultyMultiplier = 1.0f;
    void RememberState();
    void UpdateLootState();
    void PopulateSlots(const FSurvivalLoot& Loot);
    int32 GetSlotCapacity() const;

    bool bDropped = false;
    FSurvivalScavengeState State;
};
