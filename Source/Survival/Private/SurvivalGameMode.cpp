#include "SurvivalGameMode.h"

#include "CollisionQueryParams.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/HitResult.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "SurvivalBuildChunk.h"
#include "SurvivalCharacter.h"
#include "SurvivalHUD.h"
#include "SurvivalPlayerController.h"

namespace
{
const FName GroundTag(TEXT("StructuralGround"));
const FName SunTag(TEXT("CrossroadsSun"));
const FName MoonTag(TEXT("CrossroadsMoon"));
constexpr double StartHour = 8.0;
constexpr double BuildChunkSize = 1600.0;

FIntVector StructureCell(const FVector& Base)
{
    constexpr double CellSize = FSurvivalBuildPlacement::GridSize;
    return FIntVector(FMath::FloorToInt(Base.X / CellSize), FMath::FloorToInt(Base.Y / CellSize),
        FMath::FloorToInt(Base.Z / CellSize));
}
}

ASurvivalGameMode::ASurvivalGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.25f;
    DefaultPawnClass = ASurvivalCharacter::StaticClass();
    HUDClass = ASurvivalHUD::StaticClass();
    PlayerControllerClass = ASurvivalPlayerController::StaticClass();
}

void ASurvivalGameMode::BeginPlay()
{
    Super::BeginPlay();
    for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
    {
        if (It->ActorHasTag(SunTag))
        {
            Sun = *It;
            if (const UDirectionalLightComponent* Light = It->FindComponentByClass<UDirectionalLightComponent>())
            {
                DaylightIntensity = Light->Intensity;
            }
        }
        else if (It->ActorHasTag(MoonTag))
        {
            Moon = *It;
            if (const UDirectionalLightComponent* Light = It->FindComponentByClass<UDirectionalLightComponent>())
            {
                MoonlightIntensity = Light->Intensity;
            }
        }
    }
    TOptional<double> North;
    TOptional<double> City;
    TOptional<double> South;
    for (TActorIterator<ATargetPoint> It(GetWorld()); It; ++It)
    {
        const double Y = It->GetActorLocation().Y;
        if (It->ActorHasTag(FName(TEXT("CrossroadsClimateNorth"))))
        {
            North = Y;
        }
        else if (It->ActorHasTag(FName(TEXT("CrossroadsClimateCity"))))
        {
            City = Y;
        }
        else if (It->ActorHasTag(FName(TEXT("CrossroadsClimateSouth"))))
        {
            South = Y;
        }
    }
    if (North.IsSet() && City.IsSet() && South.IsSet() && North.GetValue() < City.GetValue() && City.GetValue() < South.GetValue())
    {
        ClimateBoundaries.Emplace(North.GetValue(), City.GetValue(), South.GetValue());
    }
    UpdateDaylight();
}

void ASurvivalGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    ElapsedSurvivalSeconds += DeltaSeconds;
    UpdateDaylight();
    UpdateScavengeCycle();

    constexpr int32 FirstHordeMinute = 22 * 60;
    const double Now = GetWorld()->GetTimeSeconds();
    if (!bFirstHordeStarted && Now >= NextHordeAttempt && GetWorldMinutes() >= FirstHordeMinute
        && (GetWorld()->GetMapName().EndsWith(TEXT("L_SouthernCrossroads"))
            || GetWorld()->GetMapName().EndsWith(TEXT("L_Crossroads"))
            || GetWorld()->GetMapName().EndsWith(TEXT("L_CrossroadsRelief"))))
    {
        NextHordeAttempt = Now + 2.0;
        if (ASurvivalCharacter* Player = Cast<ASurvivalCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
        {
            bFirstHordeStarted = Player->TryStartFirstHorde();
        }
    }
}

void ASurvivalGameMode::RegisterScavengeObject(ASurvivalSupplyCrate& Object)
{
    if (LootSeed == 0)
    {
        LootSeed = FMath::RandRange(1, MAX_int32 - 1);
    }
    ScavengeObjects.Add(TWeakObjectPtr<ASurvivalSupplyCrate>(&Object));
    RefreshScavengeObject(Object);
}

void ASurvivalGameMode::UnregisterScavengeObject(ASurvivalSupplyCrate& Object)
{
    ScavengeObjects.Remove(TWeakObjectPtr<ASurvivalSupplyCrate>(&Object));
}

void ASurvivalGameMode::RememberScavengeObject(FName Id, const FSurvivalScavengeState& State)
{
    ScavengeState.Add(Id, State);
}

void ASurvivalGameMode::RefreshScavengeObject(ASurvivalSupplyCrate& Object) const
{
    const FName Id = Object.GetLootId();
    const uint32 Seed = HashCombineFast(GetTypeHash(Id), HashCombineFast(LootSeed, LootCycle));
    Object.SetLootCycle(static_cast<int32>(Seed), ScavengeState.Find(Id));
}

void ASurvivalGameMode::UpdateScavengeCycle()
{
    // This clock advances with world play time and is independent of developer time-of-day changes.
    const double Period = static_cast<double>(FMath::Max(1, LootRefreshDays))
        * FMath::Max(60.0f, DayLengthSeconds);
    const int32 CurrentCycle = FMath::FloorToInt32(GetWorld()->GetTimeSeconds() / Period);
    if (CurrentCycle == LootCycle)
    {
        return;
    }

    LootCycle = CurrentCycle;
    ScavengeState.Reset();
    for (auto It = ScavengeObjects.CreateIterator(); It; ++It)
    {
        if (ASurvivalSupplyCrate* Object = It->Get())
        {
            RefreshScavengeObject(*Object);
        }
        else
        {
            It.RemoveCurrent();
        }
    }
}

int32 ASurvivalGameMode::GetWorldMinutes() const
{
    return FMath::FloorToInt32(StartHour * 60.0 + ElapsedSurvivalSeconds
        * 1440.0 / FMath::Max(60.0f, DayLengthSeconds) + 1e-6);
}

double ASurvivalGameMode::GetAmbientTemperature(const FVector& Position) const
{
    const double Hours = StartHour + ElapsedSurvivalSeconds * 24.0 / FMath::Max(60.0f, DayLengthSeconds);
    double RegionalOffset = 0.0;
    if (ClimateBoundaries.IsSet())
    {
        const FVector& Bounds = ClimateBoundaries.GetValue();
        RegionalOffset = Position.Y < Bounds.Y
            ? FMath::GetMappedRangeValueClamped(FVector2D(Bounds.X, Bounds.Y), FVector2D(-15, 0), Position.Y)
            : FMath::GetMappedRangeValueClamped(FVector2D(Bounds.Y, Bounds.Z), FVector2D(0, 14), Position.Y);
    }
    return 13.0 + RegionalOffset + 8.0 * FMath::Cos(FMath::DegreesToRadians((Hours - 14.0) * 15.0));
}

bool ASurvivalGameMode::SetTimeOfDay(double Hour)
{
    if (!FMath::IsFinite(Hour) || Hour < 0.0 || Hour >= 24.0)
    {
        return false;
    }
    const int32 Day = GetWorldMinutes() / 1440;
    const int32 Minute = FMath::Min(1439, FMath::RoundToInt32(Hour * 60.0));
    ElapsedSurvivalSeconds = (Day * 1440.0 + Minute - StartHour * 60.0) * FMath::Max(60.0f, DayLengthSeconds) / 1440.0;
    UpdateDaylight();
    return true;
}

void ASurvivalGameMode::UpdateDaylight()
{
    const double Hours = StartHour + ElapsedSurvivalSeconds * 24.0 / FMath::Max(60.0f, DayLengthSeconds);
    constexpr double Dawn = 4.0;
    constexpr double Dusk = 22.0;
    const double Hour = FMath::Fmod(Hours, 24.0);
    const double SinceDawn = FMath::Fmod(Hour - Dawn + 24.0, 24.0);
    constexpr double DaylightHours = Dusk - Dawn;
    const double SolarAngle = SinceDawn < DaylightHours
        ? SinceDawn * 180.0 / DaylightHours
        : 180.0 + (SinceDawn - DaylightHours) * 180.0 / (24.0 - DaylightHours);
    const double Elevation = FMath::Sin(FMath::DegreesToRadians(SolarAngle));
    const auto UpdateLight = [](ADirectionalLight* Actor, double Pitch, double Intensity)
    {
        if (Actor)
        {
            Actor->SetActorRotation(FRotator(Pitch, -35.0, 0.0));
            if (UDirectionalLightComponent* Light = Actor->FindComponentByClass<UDirectionalLightComponent>())
            {
                Light->SetIntensity(static_cast<float>(Intensity));
            }
        }
    };
    const double Daylight = FMath::Clamp(Elevation * 2.0, 0.0, 1.0);
    const double Moonlight = FMath::Clamp(-Elevation * 4.0, 0.0, 1.0);
    UpdateLight(Sun.Get(), -SolarAngle, Daylight * DaylightIntensity);
    UpdateLight(Moon.Get(), -SolarAngle + 180.0, Moonlight * MoonlightIntensity);
}

ASurvivalBuildChunk* ASurvivalGameMode::FindOrCreateChunk(const FVector& Base)
{
    const FIntPoint Key(FMath::FloorToInt(Base.X / BuildChunkSize), FMath::FloorToInt(Base.Y / BuildChunkSize));
    ASurvivalBuildChunk* Chunk = BuildChunks.FindRef(Key).Get();
    if (!IsValid(Chunk))
    {
        const FVector Origin(Key.X * BuildChunkSize, Key.Y * BuildChunkSize, 0.0);
        Chunk = GetWorld()->SpawnActor<ASurvivalBuildChunk>(Origin, FRotator::ZeroRotator);
        if (Chunk)
        {
            BuildChunks.Add(Key, Chunk);
        }
    }
    return Chunk;
}

bool ASurvivalGameMode::AddPlan(const FSurvivalBuildPlacement& Placement)
{
    ASurvivalBuildChunk* Chunk = FindOrCreateChunk(Placement.Base);
    if (!Chunk || !Chunk->AddPlan(Placement))
    {
        return false;
    }

    PlannedCost.Add(Placement.GetPiece().Cost);
    return true;
}

void ASurvivalGameMode::RemovePlan(const FSurvivalPlanTarget& Target)
{
    if (const FSurvivalBuildPlacement* Plan = Target.GetPlan())
    {
        PlannedCost.Spend(Plan->GetPiece().Cost);
        Target.Chunk->RemovePlan(Target.PlanIndex);
    }
}

bool ASurvivalGameMode::HasGroundFooting(const FSurvivalBuildPlacement& Placement) const
{
    const FVector Probe(0, 0, 8);
    const FCollisionQueryParams Query(SCENE_QUERY_STAT(StructuralGround), false);
    for (const FBox& Box : Placement.GetPiece().Boxes)
    {
        if (Box.Min.Z != 0.0)
        {
            continue;
        }
        for (double X : {Box.Min.X + 2.0, Box.Max.X - 2.0})
        {
            for (double Y : {Box.Min.Y + 2.0, Box.Max.Y - 2.0})
            {
                const FVector Foot = Placement.MeshOrigin() + Placement.Rotation.RotateVector(FVector(X, Y, 0));
                FHitResult Hit;
                if (!GetWorld()->LineTraceSingleByChannel(Hit, Foot + Probe, Foot - Probe, ECC_Visibility, Query)
                    || Hit.ImpactNormal.Z < 0.95 || !Hit.GetActor() || !Hit.GetActor()->ActorHasTag(GroundTag))
                {
                    return false;
                }
            }
        }
    }
    return true;
}

void ASurvivalGameMode::FindContacts(const FSurvivalBuildPlacement& Placement,
    TArray<FSurvivalSupportLink>& Supports, TArray<FSurvivalSupportLink>& Dependents) const
{
    const FIntVector Cell = StructureCell(Placement.Base);
    // Current pieces are one cell wide and at most two cells tall.
    for (int32 X = -1; X <= 1; ++X)
    {
        for (int32 Y = -1; Y <= 1; ++Y)
        {
            for (int32 Z = -3; Z <= 3; ++Z)
            {
                const TArray<int32>* Candidates = PiecesByCell.Find(Cell + FIntVector(X, Y, Z));
                if (!Candidates)
                {
                    continue;
                }
                for (int32 Id : *Candidates)
                {
                    const FSurvivalBuildPlacement& Other = RegisteredPieces.FindChecked(Id).Placement;
                    const int32 Support = Placement.SupportCostFrom(Other);
                    const int32 Dependent = Other.SupportCostFrom(Placement);
                    if (Support >= 0)
                    {
                        Supports.Add({Id, Support});
                    }
                    if (Dependent >= 0)
                    {
                        Dependents.Add({Id, Dependent});
                    }
                }
            }
        }
    }
}

bool ASurvivalGameMode::HasSupportedContact(const TArray<FSurvivalSupportLink>& Supports) const
{
    return Supports.ContainsByPredicate([this](const FSurvivalSupportLink& Link)
    {
        return Structure.GetBudget(Link.Id) >= Link.Cost;
    });
}

bool ASurvivalGameMode::HasReservedCellConflict(const FSurvivalBuildPlacement& Placement, bool bIncludePlans) const
{
    const int32 X = FMath::FloorToInt32(Placement.Base.X / BuildChunkSize);
    const int32 Y = FMath::FloorToInt32(Placement.Base.Y / BuildChunkSize);
    for (int32 DX = -1; DX <= 1; ++DX)
    {
        for (int32 DY = -1; DY <= 1; ++DY)
        {
            const ASurvivalBuildChunk* Chunk = BuildChunks.FindRef(FIntPoint(X + DX, Y + DY)).Get();
            if (Chunk && Chunk->HasReservedCellConflict(Placement, bIncludePlans))
            {
                return true;
            }
        }
    }
    return false;
}

bool ASurvivalGameMode::CanSupport(const FSurvivalBuildPlacement& Placement) const
{
    if (HasGroundFooting(Placement))
    {
        return true;
    }
    TArray<FSurvivalSupportLink> Supports, Dependents;
    FindContacts(Placement, Supports, Dependents);
    return HasSupportedContact(Supports);
}

void ASurvivalGameMode::RegisterPiece(int32 Id, ASurvivalBuildChunk& Chunk,
    const FSurvivalBuildPlacement& Placement, bool bGrounded, const TArray<FSurvivalSupportLink>& Supports,
    const TArray<FSurvivalSupportLink>& Dependents)
{
    RegisteredPieces.Add(Id, FRegisteredPiece{&Chunk, Placement});
    PiecesByCell.FindOrAdd(StructureCell(Placement.Base)).Add(Id);
    Structure.Add(Id, Placement.GetPiece().SupportCapacity(), bGrounded, Supports, Dependents);
}

bool ASurvivalGameMode::FinishPlan(const FSurvivalPlanTarget& Target, FSurvivalMaterials& Inventory)
{
    const FSurvivalBuildPlacement* Plan = Target.GetPlan();
    if (!Plan)
    {
        return false;
    }
    const FSurvivalBuildPlacement Placement = *Plan;
    const FSurvivalMaterials Cost = Placement.GetPiece().Cost;
    if (!Cost.MissingFrom(Inventory).IsEmpty())
    {
        return false;
    }
    const bool bGrounded = HasGroundFooting(Placement);
    TArray<FSurvivalSupportLink> Supports, Dependents;
    FindContacts(Placement, Supports, Dependents);
    const int32 Id = NextStructureId;
    if ((!bGrounded && !HasSupportedContact(Supports)) || !Target.Chunk->FinishPlan(Target.PlanIndex, Id))
    {
        return false;
    }

    ++NextStructureId;
    RegisterPiece(Id, *Target.Chunk, Placement, bGrounded, Supports, Dependents);
    Inventory.Spend(Cost);
    PlannedCost.Spend(Cost);
    return true;
}

bool ASurvivalGameMode::BeginAuthoredPrefab(FName PrefabId)
{
    if (PrefabId.IsNone())
    {
        return true;
    }
    if (InitializedPrefabs.Contains(PrefabId))
    {
        return false;
    }
    // Runtime build chunks retain the current pieces and any repairs after the level cell unloads.
    InitializedPrefabs.Add(PrefabId);
    return true;
}

bool ASurvivalGameMode::AddAuthoredPiece(const FSurvivalBuildPlacement& Placement)
{
    const bool bGrounded = HasGroundFooting(Placement);
    TArray<FSurvivalSupportLink> Supports, Dependents;
    FindContacts(Placement, Supports, Dependents);
    if (!bGrounded && !HasSupportedContact(Supports))
    {
        return false;
    }
    const int32 Id = NextStructureId;
    ASurvivalBuildChunk* Chunk = FindOrCreateChunk(Placement.Base);
    if (!Chunk || !Chunk->AddFinishedPiece(Placement, Id))
    {
        return false;
    }

    ++NextStructureId;
    RegisterPiece(Id, *Chunk, Placement, bGrounded, Supports, Dependents);
    return true;
}

void ASurvivalGameMode::RemoveStructure(int32 Id)
{
    if (!RegisteredPieces.Contains(Id))
    {
        return;
    }
    TArray<int32> Removed = Structure.Remove(Id);
    Removed.Add(Id);
    TMap<ASurvivalBuildChunk*, TSet<int32>> Batches;
    for (int32 PieceId : Removed)
    {
        FRegisteredPiece Piece;
        if (!RegisteredPieces.RemoveAndCopyValue(PieceId, Piece))
        {
            continue;
        }
        const FIntVector Cell = StructureCell(Piece.Placement.Base);
        TArray<int32>& Entries = PiecesByCell.FindChecked(Cell);
        Entries.RemoveSwap(PieceId);
        if (Entries.IsEmpty())
        {
            PiecesByCell.Remove(Cell);
        }
        if (ASurvivalBuildChunk* Chunk = Piece.Chunk.Get())
        {
            Batches.FindOrAdd(Chunk).Add(PieceId);
        }
    }

    int32 DebrisSlots = 12;
    for (const TPair<ASurvivalBuildChunk*, TSet<int32>>& Batch : Batches)
    {
        Batch.Key->RemoveFinishedPieces(Batch.Value, DebrisSlots);
    }
}
