#pragma once

#include "GameFramework/Actor.h"
#include "SurvivalVitals.h"
#include "SurvivalStation.generated.h"

UCLASS()
class ASurvivalStation : public AActor
{
    GENERATED_BODY()

public:
    ASurvivalStation();
    static bool IsKit(ESurvivalItem Item);
    static TConstArrayView<ESurvivalItem> Slots(ESurvivalItem Kind);
    static FVector Extent(ESurvivalItem Kind);
    ESurvivalItem Kind() const { return static_cast<ESurvivalItem>(Kit); }
    bool Installed(ESurvivalItem Module) const;
    bool SetInstalled(ESurvivalItem Module, bool bInstall);
    void Initialize(ESurvivalItem Item);
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    UPROPERTY(ReplicatedUsing=Refresh)
    uint8 Kit = static_cast<uint8>(ESurvivalItem::Campfire);
    UPROPERTY(ReplicatedUsing=Refresh)
    uint8 InstalledMask = 0;
    UFUNCTION()
    void Refresh();
    virtual void BeginPlay() override;
};
