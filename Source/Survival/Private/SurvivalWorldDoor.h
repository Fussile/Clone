#pragma once

#include "GameFramework/Actor.h"
#include "SurvivalWorldDoor.generated.h"

class UBoxComponent;
class UMaterialInterface;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class ASurvivalWorldDoor : public AActor
{
    GENERATED_BODY()

public:
    ASurvivalWorldDoor();

    UFUNCTION(BlueprintCallable, Category = "World Door")
    void ConfigureDoor(double OpeningWidth, double OpeningHeight, bool bRollUpDoor, bool bReverseDoor,
        UMaterialInterface* Finish, UMaterialInterface* Hardware);

    bool IsOpen() const { return bOpen; }
    bool TryToggle();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;

private:
    FTransform LeafPose(double Fraction) const;
    void ApplySize();
    UFUNCTION()
    void ApplyDoorState();

    UPROPERTY(VisibleAnywhere, Category = "World Door")
    TObjectPtr<USceneComponent> Pivot;

    UPROPERTY(VisibleAnywhere, Category = "World Door")
    TObjectPtr<UStaticMeshComponent> Leaf;

    UPROPERTY(VisibleAnywhere, Category = "World Door")
    TObjectPtr<UStaticMeshComponent> Handle;

    UPROPERTY(VisibleAnywhere, Category = "World Door")
    TObjectPtr<UBoxComponent> JambTarget;

    UPROPERTY(EditAnywhere, Category = "World Door", meta = (ClampMin = "85"))
    double Width = 105.0;

    UPROPERTY(EditAnywhere, Category = "World Door", meta = (ClampMin = "195"))
    double Height = 210.0;

    UPROPERTY(EditAnywhere, Category = "World Door")
    bool bRollUp = false;

    UPROPERTY(EditAnywhere, Category = "World Door")
    bool bReverse = false;

    UPROPERTY(ReplicatedUsing = ApplyDoorState)
    bool bOpen = false;
};
