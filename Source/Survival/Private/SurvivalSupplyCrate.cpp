#include "SurvivalSupplyCrate.h"
#include "SurvivalGear.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"
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
    if (bDropped) return;
    const UStaticMesh* CrateAsset = Mesh->GetStaticMesh();
    if ((bContainsSidearm || bContainsRifle) && CrateAsset &&
        CrateAsset->GetPathName() == TEXT("/Engine/BasicShapes/Cube.Cube"))
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

int32* FSurvivalLoot::StackCount(ESurvivalItem Item)
{
    switch (Item)
    {
    case ESurvivalItem::Timber: return &Materials.Timber;
    case ESurvivalItem::Stone: return &Materials.Stone;
    case ESurvivalItem::Food: return &Items.Food;
    case ESurvivalItem::Water: return &Items.Water;
    case ESurvivalItem::Cloth: return &Items.Cloth;
    case ESurvivalItem::Bandage: return &Items.Bandages;
    case ESurvivalItem::FieldDressing: return &Items.FieldDressings;
    case ESurvivalItem::Rounds: return &Items.Rounds;
    case ESurvivalItem::RifleRounds: return &Items.RifleRounds;
    case ESurvivalItem::TimberDoor: return &Items.TimberDoors;
    case ESurvivalItem::BuilderGuide: return &Items.BuilderGuides;
    case ESurvivalItem::ScavengerNotes: return &Items.ScavengerNotes;
    case ESurvivalItem::DressingSchematic: return &Items.DressingSchematics;
    default:
        const int32 Part = FSurvivalWeaponAssembly::PartIndex(Item);
        if (Part != INDEX_NONE) return &Items.RifleParts[Part];
        const int32 SidearmPart = FSurvivalWeaponAssembly::PartIndex(Item, ESurvivalItem::Sidearm);
        return SidearmPart != INDEX_NONE ? &Items.SidearmParts[SidearmPart] : nullptr;
    }
}

int32 FSurvivalLoot::Count(ESurvivalItem Item) const
{
    if (Item == ESurvivalItem::None) return GearIndex != INDEX_NONE ? 1 : 0;
    if (Item == ESurvivalItem::Rifle) return bRifle ? 1 : 0;
    if (Item == ESurvivalItem::Sidearm) return bSidearm ? 1 : 0;
    FSurvivalLoot Copy = *this;
    const int32* Stack = Copy.StackCount(Item);
    return Stack ? *Stack : 0;
}

FSurvivalLoot FSurvivalLoot::TakeStack(ESurvivalItem Item)
{
    FSurvivalLoot Result;
    if (Item == ESurvivalItem::None) Swap(Result.GearIndex, GearIndex);
    else if (Item == ESurvivalItem::Rifle) Swap(Result.bRifle, bRifle);
    else if (Item == ESurvivalItem::Sidearm) Swap(Result.bSidearm, bSidearm);
    else if (int32* Source = StackCount(Item)) Swap(*Result.StackCount(Item), *Source);
    return Result;
}

bool FSurvivalLoot::IsEmpty() const
{
    if (GearIndex != INDEX_NONE || !Materials.IsEmpty() || bSidearm || bRifle || Items.Food || Items.Water || Items.Cloth
        || Items.Bandages || Items.Rounds || Items.TimberDoors || Items.RifleRounds || Items.BuilderGuides
        || Items.ScavengerNotes || Items.DressingSchematics || Items.FieldDressings)
    {
        return false;
    }
    for (int32 Slot = 0; Slot < FSurvivalWeaponAssembly::PartCount; ++Slot)
    {
        if (Items.RifleParts[Slot] > 0 || Items.SidearmParts[Slot] > 0)
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
                    auto& Parts = Random.RandRange(0, 1) == 0 ? Loot.Items.RifleParts : Loot.Items.SidearmParts;
                    ++Parts[Random.RandRange(0, FSurvivalWeaponAssembly::PartCount - 1)];
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
    if (RiflePartSlot >= 0 && RiflePartSlot < FSurvivalWeaponAssembly::PartCount)
    {
        ++Loot.Items.RifleParts[RiflePartSlot];
    }
    Loot.Items.RifleRounds += bContainsRifle ? 8 : 0;
    if ((LootProfile == ESurvivalLootProfile::Security || LootProfile == ESurvivalLootProfile::General)
        && Random.RandRange(1, 100) <= 35)
    {
        Loot.GearIndex = Random.RandRange(0, UE_ARRAY_COUNT(SurvivalGear) - 1);
    }
    if (bContainsRifle)
    {
        // The station locker introduces the equipment screen on the first supply run.
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(SurvivalGear); ++Index)
            if (FCString::Strcmp(SurvivalGear[Index].Id, TEXT("hard_hat")) == 0) Loot.GearIndex = Index;
    }
    Loot.bSidearm = bContainsSidearm;
    Loot.bRifle = bContainsRifle;
    return Loot;
}

void ASurvivalSupplyCrate::SetLootCycle(int32 Seed, const FSurvivalScavengeState* Previous)
{
    FRandomStream Random(Seed);
    const bool bOccupied = Random.RandRange(1, 100) <= SpawnChancePercent;
    State = Previous ? *Previous : FSurvivalScavengeState{MaxHealth, false, {}};
    bWorldPresent = bOccupied && State.Health > 0.0f;
    Contents = {};
    if (bWorldPresent && !State.bSearched)
        Contents = State.Remaining.IsSet() ? State.Remaining.GetValue() : RollLoot(Random);
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

FSurvivalLoot ASurvivalSupplyCrate::TakeLoot(bool bTakeAll, ESurvivalItem Item)
{
    if (!HasAuthority() || !bLootAvailable) return {};
    FSurvivalLoot Loot;
    if (bTakeAll) Swap(Loot, Contents);
    else Loot = Contents.TakeStack(Item);
    if (Loot.IsEmpty()) return {};

    bLootAvailable = !Contents.IsEmpty();
    bSidearmAvailable = Contents.bSidearm;
    bRifleAvailable = Contents.bRifle;
    if (bDropped)
    {
        if (!bLootAvailable) Destroy();
        return Loot;
    }

    State.bSearched = !bLootAvailable;
    if (bLootAvailable) State.Remaining = Contents;
    else State.Remaining.Reset();
    RememberState();
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
        State.Remaining.Reset();
        bWorldPresent = bLootAvailable = bSidearmAvailable = bRifleAvailable = false;
        OnRep_WorldPresence();
    }
    RememberState();
    return Applied;
}

bool ASurvivalSupplyCrate::Drop(ACharacter& Owner, const FSurvivalLoot& Loot)
{
    if (!Owner.HasAuthority() || Loot.IsEmpty()) return false;
    UWorld* World = Owner.GetWorld();
    const FVector Start = Owner.GetActorLocation();
    const FVector Ahead = Start + Owner.GetActorForwardVector() * 85;
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(DropLoot), false, &Owner);
    if (World->LineTraceTestByChannel(Start, Ahead, ECC_Visibility, Query) ||
        !World->LineTraceSingleByChannel(Hit, Ahead, Ahead - FVector(0,0,250), ECC_Visibility, Query))
        return false;
    const FTransform Pose(FQuat::Identity, Hit.ImpactPoint + FVector(0,0,12), FVector(.3,.3,.24));
    auto* Crate = World->SpawnActorDeferred<ASurvivalSupplyCrate>(StaticClass(), Pose, nullptr, &Owner,
        ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding);
    if (!Crate) return false;
    Crate->bDropped = true;
    Crate->Contents = Loot;
    Crate->bLootAvailable = true;
    Crate->bWorldPresent = true;
    Crate->FinishSpawning(Pose);
    return IsValid(Crate);
}
