#pragma once

#include "GameFramework/Actor.h"
#include "SurvivalStructurePrefab.generated.h"

USTRUCT()
struct FSurvivalPrefabPart
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Structure")
    FVector Offset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, Category = "Structure")
    int32 PieceIndex = 0;

    UPROPERTY(EditAnywhere, Category = "Structure")
    int32 QuarterTurns = 0;
};

UCLASS()
class ASurvivalStructurePrefab : public AActor
{
    GENERATED_BODY()

public:
    ASurvivalStructurePrefab();

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(EditAnywhere, Category = "Structure")
    FName PrefabId;

    UPROPERTY(EditAnywhere, Category = "Structure")
    TArray<FSurvivalPrefabPart> Parts;
};
