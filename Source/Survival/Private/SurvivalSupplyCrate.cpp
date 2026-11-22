#include "SurvivalSupplyCrate.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalGameMode.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
enum class ELootGroup : uint8 { Food, Building, Medical, Security, Books, Count };

struct FLootWeights
{
    int32 Values[static_cast<int32>(ELootGroup::Count)];
};

constexpr FLootWeights General{{30, 30, 18, 10, 12}};
constexpr FLootWeights Food{{78, 8, 7, 2, 5}};
constexpr FLootWeights Building{{9, 73, 8, 6, 4}};
constexpr FLootWeights Medical{{9, 6, 76, 3, 6}};
constexpr FLootWeights Security{{9, 12, 15, 59, 5}};
constexpr FLootWeights Office{{19, 15, 13, 5, 48}};

const FLootWeights& WeightsFor(ESurvivalLootProfile Profile)
{
    switch (Profile)
    {
    case ESurvivalLootProfile::Food: return Food;
    case ESurvivalLootProfile::Building: return Building;
    case ESurvivalLootProfile::Medical: return Medical;
    case ESurvivalLootProfile::Security: return Security;
    case ESurvivalLootProfile::Office: return Office;
    default: return General;
    }
}

ELootGroup PickGroup(FRandomStream& Random, const FLootWeights& Weights)
{
    int32 Roll = Random.RandRange(1, 100);
    for (int32 Index = 0; Index < static_cast<int32>(ELootGroup::Count); ++Index)
    {
        Roll -= Weights.Values[Index];
        if (Roll <= 0)
        {
            return static_cast<ELootGroup>(Index);
        }
    }
    return ELootGroup::Food;
}
}

ASurvivalSupplyCrate::ASurvivalSupplyCrate()
{
    bReplicates = true;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CrateMesh"));
    SetRootComponent(Mesh);
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cube.Succeeded())
    {
        Mesh->SetStaticMesh(Cube.Object);
    }
}

void ASurvivalSupplyCrate::BeginPlay()
{
    Super::BeginPlay();
    if (bContainsSidearm || bContainsRifle)
    {
        if (UMaterialInterface* Steel = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Art/Crossroads/Building/M_DefenseSteel.M_DefenseSteel")))
        {
            Mesh->SetMaterial(0, Steel);
        }
    }

    if (ASurvivalGameMode* Mode = GetWorld()->GetAuthGameMode<ASurvivalGameMode>())
    {
        Mode->RegisterScavengeObject(*this);
    }
    else if (HasAuthority())
    {
        SetLootCycle(GetTypeHash(GetLootId()), nullptr);
    }
}

void ASurvivalSupplyCrate::EndPlay(const EEndPlayReason::Type Reason)
{
    if (ASurvivalGameMode* Mode = GetWorld()->GetAuthGameMode<ASurvivalGameMode>())
    {
        Mode->UnregisterScavengeObject(*this);
    }
    Super::EndPlay(Reason);
}

void ASurvivalSupplyCrate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASurvivalSupplyCrate, bWorldPresent);
    DOREPLIFETIME(ASurvivalSupplyCrate, bLootAvailable);
    DOREPLIFETIME(ASurvivalSupplyCrate, bSidearmAvailable);
    DOREPLIFETIME(ASurvivalSupplyCrate, bRifleAvailable);
}

FName ASurvivalSupplyCrate::GetLootId() const
{
    // The regional baker supplies a unique tag that survives World Partition streaming.
    return Tags.IsEmpty() ? FName(*GetPathName()) : Tags[0];
}

bool FSurvivalLoot::IsEmpty() const
{
    if (!Materials.IsEmpty() || bSidearm || bRifle || Items.Food || Items.Water || Items.Cloth
        || Items.Bandages || Items.Rounds || Items.TimberDoors || Items.RifleRounds || Items.BuilderGuides
        || Items.ScavengerNotes || Items.DressingSchematics || Items.FieldDressings)
    {
        return false;
    }
    for (int32 Parts : Items.RifleParts)
    {
        if (Parts > 0)
        {
            return false;
        }
    }
    return true;
}

FSurvivalLoot ASurvivalSupplyCrate::RollLoot(FRandomStream& Random) const
{
    FSurvivalLoot Loot;
    if (LootProfile == ESurvivalLootProfile::Authored)
    {
        Loot.Materials = {FMath::Max(0, TimberAmount), FMath::Max(0, StoneAmount)};
        Loot.Items.Food = FMath::Max(0, FoodAmount);
        Loot.Items.Water = FMath::Max(0, WaterAmount);
        Loot.Items.Cloth = FMath::Max(0, ClothAmount);
    }
    else
    {
        const int32 Draws = Random.RandRange(1, 2);
        for (int32 Index = 0; Index < Draws; ++Index)
        {
            switch (PickGroup(Random, WeightsFor(LootProfile)))
            {
            case ELootGroup::Food:
                Loot.Items.Food += Random.RandRange(1, 3);
                Loot.Items.Water += Random.RandRange(1, 2);
                break;
            case ELootGroup::Building:
                Loot.Materials.Timber += Random.RandRange(3, 9);
                Loot.Materials.Stone += Random.RandRange(1, 5);
                break;
            case ELootGroup::Medical:
                Loot.Items.Cloth += Random.RandRange(1, 4);
                if (Random.RandRange(1, 4) == 1)
                {
                    ++Loot.Items.Bandages;
                }
                break;
            case ELootGroup::Security:
                Loot.Items.Rounds += Random.RandRange(3, 7);
                Loot.Items.RifleRounds += Random.RandRange(1, 3);
                if (Random.RandRange(1, 5) == 1)
                {
                    ++Loot.Items.RifleParts[Random.RandRange(0, FSurvivalRifle::PartCount - 1)];
                }
                break;
            case ELootGroup::Books:
                if (Random.RandRange(1, 3) == 1)
                {
                    ++Loot.Items.BuilderGuides;
                }
                else
                {
                    ++Loot.Items.ScavengerNotes;
                }
                break;
            default: break;
            }
        }
    }
    Loot.Items.BuilderGuides += FMath::Max(0, BuilderGuideAmount);
    Loot.Items.ScavengerNotes += FMath::Max(0, ScavengerNotesAmount);
    Loot.Items.DressingSchematics += FMath::Max(0, DressingSchematicAmount);
    if (RiflePartSlot >= 0 && RiflePartSlot < FSurvivalRifle::PartCount)
    {
        ++Loot.Items.RifleParts[RiflePartSlot];
    }
    Loot.Items.RifleRounds += bContainsRifle ? 8 : 0;
    Loot.bSidearm = bContainsSidearm;
    Loot.bRifle = bContainsRifle;
    return Loot;
}

void ASurvivalSupplyCrate::SetLootCycle(int32 Seed, const FSurvivalScavengeState* Previous)
{
    FRandomStream Random(Seed);
    const bool bOccupied = Random.RandRange(1, 100) <= SpawnChancePercent;
    State = Previous ? *Previous : FSurvivalScavengeState{MaxHealth, false};
    bWorldPresent = bOccupied && State.Health > 0.0f;
    Contents = bWorldPresent && !State.bSearched ? RollLoot(Random) : FSurvivalLoot{};
    bLootAvailable = !Contents.IsEmpty();
    bSidearmAvailable = Contents.bSidearm;
    bRifleAvailable = Contents.bRifle;
    OnRep_WorldPresence();
    ForceNetUpdate();
}

void ASurvivalSupplyCrate::OnRep_WorldPresence()
{
    Mesh->SetVisibility(bWorldPresent, true);
    SetActorEnableCollision(bWorldPresent);
}

void ASurvivalSupplyCrate::RememberState()
{
    if (ASurvivalGameMode* Mode = GetWorld()->GetAuthGameMode<ASurvivalGameMode>())
    {
        Mode->RememberScavengeObject(GetLootId(), State);
    }
    ForceNetUpdate();
}

FSurvivalLoot ASurvivalSupplyCrate::TakeLoot()
{
    if (!HasAuthority() || !bLootAvailable)
    {
        return {};
    }
    State.bSearched = true;
    bLootAvailable = bSidearmAvailable = bRifleAvailable = false;
    RememberState();
    FSurvivalLoot Loot = Contents;
    Contents = {};
    return Loot;
}

float ASurvivalSupplyCrate::TakeDamage(float Damage, const FDamageEvent&, AController*, AActor*)
{
    if (!HasAuthority() || !bWorldPresent || !FMath::IsFinite(Damage) || Damage <= 0.0f)
    {
        return 0.0f;
    }
    const float Applied = FMath::Min(Damage, State.Health);
    State.Health -= Applied;
    if (State.Health <= 0.0f)
    {
        Contents = {};
        bWorldPresent = bLootAvailable = bSidearmAvailable = bRifleAvailable = false;
        OnRep_WorldPresence();
    }
    RememberState();
    return Applied;
}
