#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SurvivalQuarantineMine.generated.h"

UCLASS()
class ASurvivalQuarantineMine : public AActor
{
    GENERATED_BODY()
public:
    ASurvivalQuarantineMine();
    virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
private:
    bool bSpent = false;
};
