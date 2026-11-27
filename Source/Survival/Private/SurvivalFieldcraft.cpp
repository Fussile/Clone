#include "SurvivalCharacter.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "SurvivalBuildChunk.h"
#include "SurvivalGear.h"
#include "SurvivalStation.h"
#include "SurvivalZombie.h"
#include "SurvivalWorldDoor.h"

namespace
{
using I = ESurvivalItem;
constexpr double FieldReach = 260.;

// Re-trace to the reported contact on authority. The extended end reaches the far side of network rounding.
bool ConfirmFieldContact(const ASurvivalCharacter& Player, const FVector& Point, FHitResult& Hit)
{
    FVector Eye;
    FRotator Rotation;
    Player.GetActorEyesViewPoint(Eye, Rotation);
    const FVector Ray = Point - Eye;
    if (Point.ContainsNaN() || Ray.SizeSquared() > FMath::Square(FieldReach + 6.)) return false;
    return Player.GetWorld()->LineTraceSingleByChannel(Hit, Eye, Point + Ray.GetSafeNormal() * 4., ECC_Visibility,
        FCollisionQueryParams(SCENE_QUERY_STAT(WorkshopContact), false, &Player)) &&
        FVector::DistSquared(Hit.ImpactPoint, Point) < 64.;
}

bool CanReachStation(const ASurvivalCharacter& Player, ASurvivalStation* Station, const FVector& Point)
{
    if (!IsValid(Station) || Point.ContainsNaN() || !Station->GetComponentsBoundingBox().ExpandBy(3.).IsInsideOrOn(Point)) return false;
    FVector Eye;
    FRotator Rotation;
    Player.GetActorEyesViewPoint(Eye, Rotation);
    if (FVector::DistSquared(Eye, Point) > FMath::Square(FieldReach + 6.)) return false;
    FHitResult Hit;
    return !Player.GetWorld()->LineTraceSingleByChannel(Hit, Eye, Point, ECC_Visibility,
        FCollisionQueryParams(SCENE_QUERY_STAT(WorkshopAccess), false, &Player)) || Hit.GetActor() == Station;
}

bool IsTree(const FHitResult& Hit)
{
    return Hit.Item != INDEX_NONE && IsValid(Hit.GetActor()) && Hit.GetActor()->Tags.ContainsByPredicate([](FName Tag)
    {
        const FString Name = Tag.ToString();
        return Name.StartsWith(TEXT("CrossroadsVegetation_")) && Name.EndsWith(TEXT("_Trunks"));
    });
}

int32 FiberGear(I Item)
{
    if (Item == I::FiberShirt) return UE_ARRAY_COUNT(SurvivalGear) - 2;
    if (Item == I::FiberPants) return UE_ARRAY_COUNT(SurvivalGear) - 1;
    return INDEX_NONE;
}
}

void ASurvivalCharacter::FieldMessage(const FString& Text) const
{
    if (auto* Player = Cast<APlayerController>(GetController())) Player->ClientMessage(Text);
}

bool ASurvivalCharacter::FindFieldHit(FHitResult& Hit) const
{
    return FindWorldAimHit(FieldReach, Hit) && HasReachTo(Hit.ImpactPoint, FieldReach);
}

FString ASurvivalCharacter::FieldPrompt() const
{
    if (bBuildMode || bInventoryOpen || HealthPoints <= 0) return {};
    FHitResult Hit;
    if (FindFieldHit(Hit) && IsValid(Hit.GetActor()))
    {
        if (const auto* Station = Cast<ASurvivalStation>(Hit.GetActor()))
            return FString::Printf(TEXT("[E] %s%s"), Station->Kind() == I::Bedroll ? TEXT("Rest at ") : TEXT("Use "), SurvivalItemName(Station->Kind()));
        if (IsTree(Hit)) return TEXT("[E] Gather wood and fibers");
        if (GetHotbarItem(ActiveHotbarSlot) == I::Hammer)
            if (auto* Chunk = Cast<ASurvivalBuildChunk>(Hit.GetActor()))
                if (const auto* Piece = Chunk->FindFinished(Hit)) return Piece->UpgradePrompt();
        if (Cast<ASurvivalWorldDoor>(Hit.GetActor()) || Cast<ASurvivalSupplyCrate>(Hit.GetActor()) ||
            Cast<ASurvivalBuildChunk>(Hit.GetActor()) || Hit.GetActor()->ActorHasTag(TEXT("CrossroadsRoofLadder"))) return {};
    }
    if (bTouchingWater && !bHeadUnderwater && Vitals.Thirst < FSurvivalVitals::Max) return TEXT("[E] Drink untreated water (high illness risk)");
    return {};
}

bool ASurvivalCharacter::FieldInteract()
{
    if (bBuildMode) return false;
    FHitResult Hit;
    if (FindFieldHit(Hit) && IsValid(Hit.GetActor()))
    {
        if (auto* Station = Cast<ASurvivalStation>(Hit.GetActor()))
        {
            if (Station->Kind() == I::Bedroll)
            {
                if (GetWorld()->GetTimeSeconds() >= NextRest)
                {
                    NextRest = GetWorld()->GetTimeSeconds() + 3.;
                    Vitals.Stamina = FMath::Min(FSurvivalVitals::Max, Vitals.Stamina + 15.f);
                    FieldMessage(TEXT("Rested at the bedroll."));
                }
                return true;
            }
            OpenStation = Station;
            StationContact = Hit.ImpactPoint;
            SetInventoryOpen(true);
            return true;
        }
        if (IsTree(Hit))
        {
            const bool bAxe = GetHotbarItem(ActiveHotbarSlot) == I::StoneAxe;
            ServerPrimitiveHit(Hit.GetActor(), FVector_NetQuantize10(Hit.ImpactPoint), static_cast<uint8>(bAxe ? I::StoneAxe : I::None));
            return true;
        }
        if (Cast<ASurvivalBuildChunk>(Hit.GetActor()) || Cast<ASurvivalSupplyCrate>(Hit.GetActor()) ||
            Cast<ASurvivalWorldDoor>(Hit.GetActor()) || Hit.GetActor()->ActorHasTag(TEXT("CrossroadsRoofLadder"))) return false;
    }
    if (!bTouchingWater || bHeadUnderwater || Vitals.Thirst >= FSurvivalVitals::Max || FindPlan().GetPlan()) return false;
    if (GetWorld()->GetTimeSeconds() >= NextFieldAction)
    {
        NextFieldAction = GetWorld()->GetTimeSeconds() + 1.;
        DrinkFieldWater(I::RawWater);
    }
    return true;
}

void ASurvivalCharacter::DrinkFieldWater(I Quality)
{
    const bool bClean = Quality == I::Water;
    Vitals.Thirst = FMath::Min(FSurvivalVitals::Max, Vitals.Thirst + (bClean ? 42.f : 20.f));
    if (!bClean && FMath::FRand() < SurvivalWaterIllnessChance(Quality))
    {
        WaterIllness = 60.f;
        FieldMessage(TEXT("Waterborne illness: losing hydration and food for one minute."));
    }
}

bool ASurvivalCharacter::StationInReach() const
{
    if (!OpenStation.IsValid() || !bInventoryOpen || HealthPoints <= 0) return false;
    return CanReachStation(*this, OpenStation.Get(), StationContact);
}

ESurvivalItem ASurvivalCharacter::StationSlot(int32 Index) const
{
    if (!OpenStation.IsValid()) return I::None;
    const auto Options = ASurvivalStation::Slots(OpenStation->Kind());
    return Options.IsValidIndex(Index) ? Options[Index] : I::None;
}

FString ASurvivalCharacter::StationStatus() const
{
    if (!OpenStation.IsValid()) return {};
    FString Status = SurvivalItemName(OpenStation->Kind());
    if (!StationInReach()) return Status + TEXT(" (out of reach)");
    for (auto Slot : ASurvivalStation::Slots(OpenStation->Kind()))
        if (OpenStation->Installed(Slot)) Status += FString::Printf(TEXT("  /  %s"), SurvivalItemName(Slot));
    return Status;
}

bool ASurvivalCharacter::CanToggleModule(int32 Index) const
{
    const auto Module = StationSlot(Index);
    if (bFieldRequestPending || Module == I::None || !StationInReach()) return false;
    return OpenStation->Installed(Module) ? HasRoomForExchange({}, Module, 1) : GetItemCount(Module) > 0;
}

void ASurvivalCharacter::ToggleModule(int32 Index)
{
    if (!CanToggleModule(Index)) return;
    const auto Module = StationSlot(Index);
    const bool bInstall = !OpenStation->Installed(Module);
    bFieldRequestPending = true;
    if (bInstall)
    {
        PendingFieldItem = Module;
        ChangeItemCount(Module, -1);
        SyncPack();
    }
    ServerChangeModule(OpenStation.Get(), static_cast<uint8>(Module), bInstall, FVector_NetQuantize10(StationContact));
}

void ASurvivalCharacter::ServerChangeModule_Implementation(ASurvivalStation* Station, uint8 ModuleId, bool bInstall, FVector_NetQuantize10 Point)
{
    const bool bOK = HealthPoints > 0 && CanReachStation(*this, Station, Point) &&
        Station->SetInstalled(static_cast<I>(ModuleId), bInstall);
    ClientFieldResult(bOK, bOK && !bInstall ? ModuleId : static_cast<uint8>(I::None));
}

int32 ASurvivalCharacter::IngredientCost(const FSurvivalRecipe& Recipe, const FSurvivalIngredient& Ingredient, int32 Quantity) const
{
    const int32 Cost = Ingredient.Count * Quantity;
    if (OpenStation.IsValid() && OpenStation->Kind() == Recipe.Station)
    {
        if ((Recipe.Station == I::Forge && Ingredient.Item == I::Timber && OpenStation->Installed(I::Bellows)) ||
            (Recipe.Station == I::Stove && Ingredient.Item == I::FuelCell && OpenStation->Installed(I::PowerSupply)))
            return (Cost + 1) / 2;
    }
    return Cost;
}

void ASurvivalCharacter::PlaceKit(I Item)
{
    FHitResult Hit;
    if (!FindFieldHit(Hit) || Hit.ImpactNormal.Z < .85f || FVector::Dist2D(GetActorLocation(), Hit.ImpactPoint) < 80.)
    {
        FieldMessage(TEXT("Aim at level ground a step away."));
        return;
    }
    bFieldRequestPending = true;
    PendingFieldItem = Item;
    auto& Belt = Toolbelt[ActiveHotbarSlot];
    if (Belt.Item == Item && --Belt.Count == 0) Belt = {};
    ChangeItemCount(Item, -1);
    SyncPack();
    ServerPlaceKit(static_cast<uint8>(Item), FVector_NetQuantize10(Hit.ImpactPoint));
}

void ASurvivalCharacter::ServerPlaceKit_Implementation(uint8 ItemId, FVector_NetQuantize10 Point)
{
    const auto Item = static_cast<I>(ItemId);
    FHitResult Hit;
    ASurvivalStation* Placed = nullptr;
    if (HealthPoints > 0 && ASurvivalStation::IsKit(Item) && ConfirmFieldContact(*this, Point, Hit) && Hit.ImpactNormal.Z >= .85f)
    {
        const FRotator Facing(0, GetActorRotation().Yaw, 0);
        const FVector Box = ASurvivalStation::Extent(Item);
        const FVector Base = Hit.ImpactPoint + FVector(0,0,2);
        if (!GetWorld()->OverlapBlockingTestByChannel(Base + FVector(0,0,Box.Z), Facing.Quaternion(), ECC_Pawn,
            FCollisionShape::MakeBox(Box)))
        {
            Placed = GetWorld()->SpawnActor<ASurvivalStation>(Base, Facing);
            if (Placed) Placed->Initialize(Item);
        }
    }
    ClientFieldResult(Placed != nullptr, static_cast<uint8>(I::None));
}

void ASurvivalCharacter::ClientFieldResult_Implementation(bool bSuccess, uint8 ReturnId)
{
    if (!bFieldRequestPending) return;
    const auto Returned = bSuccess ? static_cast<I>(ReturnId) : PendingFieldItem;
    if (Returned != I::None) ChangeItemCount(Returned, bSuccess ? 1 : PendingMaterialCount);
    PendingMaterialCount = 1;
    if (!bSuccess) FieldMessage(TEXT("Action could not be completed."));
    bFieldRequestPending = false;
    PendingFieldItem = I::None;
    SyncPack();
}

void ASurvivalCharacter::ServerWearFiber_Implementation(uint8 ItemId)
{
    const int32 Gear = FiberGear(static_cast<I>(ItemId));
    const bool bOK = Gear != INDEX_NONE && HealthPoints > 0;
    if (bOK)
    {
        OwnedGear.Add(Gear);
        ServerSetGearEquipped_Implementation(Gear, true);
        ForceNetUpdate();
    }
    ClientFieldResult(bOK, static_cast<uint8>(I::None));
}

void ASurvivalCharacter::UsePrimitiveTool(I Tool)
{
    FHitResult Hit;
    ToolSwingStart = GetWorld()->GetTimeSeconds();
    if (FindFieldHit(Hit)) ServerPrimitiveHit(Hit.GetActor(), FVector_NetQuantize10(Hit.ImpactPoint), static_cast<uint8>(Tool));
}

void ASurvivalCharacter::ServerPrimitiveHit_Implementation(AActor* Target, FVector_NetQuantize10 Point, uint8 ToolId)
{
    const auto Tool = static_cast<I>(ToolId);
    FHitResult Hit;
    const double Now = GetWorld()->GetTimeSeconds();
    if (!IsValid(Target) || HealthPoints <= 0 || Now < NextServerFieldAction ||
        (Tool != I::None && Tool != I::StoneAxe && Tool != I::StoneSpear) ||
        !ConfirmFieldContact(*this, Point, Hit) || Hit.GetActor() != Target) return;
    if (IsTree(Hit) && Tool != I::StoneSpear)
    {
        NextServerFieldAction = Now + (Tool == I::StoneAxe ? 1.5 : 3.);
        ClientGather(Tool == I::StoneAxe ? 4 : 1, 1);
    }
    else if (auto* Zombie = Cast<ASurvivalZombie>(Target); Zombie && Tool != I::None)
    {
        NextServerFieldAction = Now + .85;
        Zombie->HitByShot(this, Tool == I::StoneSpear ? 24 : 18);
    }
}

void ASurvivalCharacter::ClientGather_Implementation(int32 Timber, int32 Fiber)
{
    for (const FSurvivalIngredient Resource : {FSurvivalIngredient{I::Timber, Timber}, FSurvivalIngredient{I::PlantFiber, Fiber}})
    {
        if (!HasRoomForExchange({}, Resource.Item, Resource.Count)) continue;
        ChangeItemCount(Resource.Item, Resource.Count);
        SyncPack();
        FieldMessage(FString::Printf(TEXT("+%d %s"), Resource.Count, SurvivalItemName(Resource.Item)));
    }
}

void ASurvivalCharacter::HammerUpgrade()
{
    FHitResult Hit;
    if (bFieldRequestPending || !FindFieldHit(Hit)) return;
    auto* Chunk = Cast<ASurvivalBuildChunk>(Hit.GetActor());
    const auto* Piece = Chunk ? Chunk->FindFinished(Hit) : nullptr;
    const auto* Next = Piece ? Piece->NextTier() : nullptr;
    if (!Next) return;
    if (GetItemCount(Next->Material) < Next->Cost)
    {
        FieldMessage(Piece->UpgradePrompt());
        return;
    }
    bFieldRequestPending = true;
    ToolSwingStart = GetWorld()->GetTimeSeconds();
    PendingFieldItem = Next->Material;
    PendingMaterialCount = Next->Cost;
    ChangeItemCount(Next->Material, -Next->Cost);
    SyncPack();
    ServerHammer(Chunk, Piece->StructureId, Piece->Tier, FVector_NetQuantize10(Hit.ImpactPoint));
}

void ASurvivalCharacter::ServerHammer_Implementation(ASurvivalBuildChunk* Chunk, int32 Id, int32 Tier, FVector_NetQuantize10 Point)
{
    FHitResult Hit;
    const bool bOK = IsValid(Chunk) && HealthPoints > 0 && ConfirmFieldContact(*this, Point, Hit)
        && Hit.GetActor() == Chunk && Chunk->Upgrade(Hit, Id, Tier);
    ClientFieldResult(bOK, static_cast<uint8>(I::None));
}

void ASurvivalCharacter::ServerUseBuiltPiece_Implementation(ASurvivalBuildChunk* Chunk, int32 Id, uint8 Action, FVector_NetQuantize10 Point)
{
    bool bOK = false;
    FHitResult Hit;
    if (HealthPoints > 0 && IsValid(Chunk) && ConfirmFieldContact(*this, Point, Hit) && Hit.GetActor() == Chunk)
    {
        const auto* Piece = Chunk->FindFinished(Hit);
        if (Piece && Piece->StructureId == Id && Piece->Tier != INDEX_NONE)
        {
            if (Action == 0) bOK = Chunk->TryToggleDoor(Hit, GetActorLocation());
            if (Action == 1) bOK = Chunk->TryInstallDoor(Hit);
            if (Action == 2)
            {
                FSurvivalMaterials Reserved = Piece->RepairCost();
                bOK = Chunk->RepairFinished(Hit, Reserved, 30);
            }
        }
    }
    ClientFieldResult(bOK, static_cast<uint8>(I::None));
}
