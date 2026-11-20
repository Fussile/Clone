#pragma once

#include "CoreMinimal.h"

struct FSurvivalWallPlacement
{
    static constexpr double GridSize = 100.0;
    static inline const FVector HalfExtent{50.0, 12.0, 100.0};

    FVector Center = FVector::ZeroVector;
    FQuat Rotation = FQuat::Identity;
    bool bClear = false;
};
