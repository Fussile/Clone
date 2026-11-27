#include "SurvivalStation.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

ASurvivalStation::ASurvivalStation()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("StationOrigin")));
    bReplicates = true;
}

bool ASurvivalStation::IsKit(ESurvivalItem Item)
{
    return Item == ESurvivalItem::Bedroll || (Item >= ESurvivalItem::ChemSet && Item <= ESurvivalItem::Printer);
}

TConstArrayView<ESurvivalItem> ASurvivalStation::Slots(ESurvivalItem Kind)
{
    using I = ESurvivalItem;
    static constexpr I Chemistry[]{I::Catalyst, I::Decanter, I::Chamber};
    static constexpr I Machining[]{I::Lathe, I::Sawmill, I::Mill};
    static constexpr I Forge[]{I::Bellows, I::FieldCrucible, I::Mold};
    static constexpr I Fire[]{I::Pot, I::Grill};
    static constexpr I Stove[]{I::Oven, I::Pot, I::PowerSupply};
    static constexpr I Printer[]{I::PowerSupply};
    switch (Kind)
    {
    case I::ChemSet: return MakeArrayView(Chemistry);
    case I::MachineBench: return MakeArrayView(Machining);
    case I::Forge: return MakeArrayView(Forge);
    case I::Campfire: return MakeArrayView(Fire);
    case I::Stove: return MakeArrayView(Stove);
    case I::Printer: return MakeArrayView(Printer);
    default: return {};
    }
}

bool ASurvivalStation::Installed(ESurvivalItem Module) const
{
    const int32 Index = Slots(Kind()).IndexOfByKey(Module);
    return Index != INDEX_NONE && (InstalledMask & (1 << Index)) != 0;
}

bool ASurvivalStation::SetInstalled(ESurvivalItem Module, bool bInstall)
{
    const int32 Index = Slots(Kind()).IndexOfByKey(Module);
    if (!HasAuthority() || Index == INDEX_NONE || Installed(Module) == bInstall) return false;
    InstalledMask ^= 1 << Index;
    Refresh();
    ForceNetUpdate();
    return true;
}

FVector ASurvivalStation::Extent(ESurvivalItem Kind)
{
    using I = ESurvivalItem;
    switch (Kind)
    {
    case I::Bedroll: return FVector(92,35,8);
    case I::Campfire: return FVector(35,35,23);
    case I::Forge: return FVector(55,47,54);
    case I::MachineBench: case I::ChemSet: return FVector(80,37,63);
    case I::Printer: return FVector(44,36,46);
    default: return FVector(40,37,47);
    }
}

void ASurvivalStation::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Props) const
{
    Super::GetLifetimeReplicatedProps(Props);
    DOREPLIFETIME(ASurvivalStation, Kit);
    DOREPLIFETIME(ASurvivalStation, InstalledMask);
}

void ASurvivalStation::BeginPlay()
{
    Super::BeginPlay();
    Refresh();
}

void ASurvivalStation::Initialize(ESurvivalItem Item)
{
    Kit = static_cast<uint8>(Item);
    Refresh();
    ForceNetUpdate();
}

void ASurvivalStation::Refresh()
{
    TInlineComponentArray<UStaticMeshComponent*> Previous(this);
    for (auto* Part : Previous) Part->DestroyComponent();
    auto* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (!Cube || !Cylinder) return;
    using I = ESurvivalItem;
    const auto Part = [&](FVector Center, FVector Size, FLinearColor Tint, bool bRound = false)
    {
        auto* Mesh = NewObject<UStaticMeshComponent>(this);
        AddInstanceComponent(Mesh);
        Mesh->SetupAttachment(GetRootComponent());
        Mesh->SetStaticMesh(bRound ? Cylinder : Cube);
        Mesh->SetRelativeLocation(Center);
        Mesh->SetRelativeScale3D(Size / 100.);
        Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
        if (auto* Mat = Mesh->CreateDynamicMaterialInstance(0)) Mat->SetVectorParameterValue(TEXT("Color"), Tint);
        Mesh->RegisterComponent();
    };
    const FLinearColor Steel(.18f,.21f,.22f), Dark(.045f,.052f,.05f), Wood(.25f,.14f,.075f);
    const auto KitKind = Kind();
    if (KitKind == I::Bedroll)
    {
        Part({0,0,4},{182,69,8},{.18f,.23f,.12f});
        Part({70,0,11},{28,61,9},{.25f,.27f,.18f});
    }
    else if (KitKind == I::Campfire)
    {
        for (int32 N = 0; N < 8; ++N)
            Part(FVector(26,0,8).RotateAngleAxis(45.f*N,FVector::UpVector),{17,17,16},Steel,true);
        Part({0,0,6},{40,12,12},Wood);
        Part({0,0,14},{12,40,10},Wood);
    }
    else if (KitKind == I::ChemSet || KitKind == I::MachineBench)
    {
        for (int32 X : {-67,67}) for (int32 Y : {-28,28})
            Part(FVector(X,Y,44),{8,8,88},Steel);
        Part({0,0,89},{160,74,6},KitKind == I::ChemSet ? Steel : Wood);
        Part({0,18,99},{100,18,14},Dark);
    }
    else if (KitKind == I::Printer)
    {
        Part({0,0,6},{86,70,12},Dark);
        for (int32 X : {-36,36}) Part(FVector(X,12,48),{6,6,78},Steel);
        Part({0,12,84},{78,8,8},Steel);
        Part({0,12,71},{13,13,18},Dark);
    }
    else if (KitKind == I::Forge)
    {
        Part({0,0,27},{105,89,54},Dark);
        for (int32 X : {-41,41}) Part(FVector(X,0,67),{20,88,29},Steel);
        Part({0,0,85},{104,88,10},Dark);
        Part({30,20,97},{20,20,21},Steel,true);
    }
    else
    {
        Part({0,0,42},{79,73,84},Steel);
        for (int32 X : {-18,18}) for (int32 Y : {-17,17})
            Part(FVector(X,Y,85),{23,23,3},Dark,true);
    }
    const auto Options = Slots(KitKind);
    for (int32 Index = 0; Index < Options.Num(); ++Index)
    {
        if (!Installed(Options[Index])) continue;
        const auto Bounds = Extent(KitKind);
        const double X = (Index - (Options.Num()-1)*.5) * Bounds.X * .55;
        const double Base = KitKind == I::Campfire ? 16 : KitKind == I::Printer ? 12 : KitKind == I::Stove ? 87 : 92;
        Part({X,-9,Base+7},{22,21,14},Steel,Options[Index] == I::Pot || Options[Index] == I::Decanter);
    }
}
