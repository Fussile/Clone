#pragma once

#include "GameFramework/HUD.h"
#include "SurvivalHUD.generated.h"

UCLASS()
class ASurvivalHUD : public AHUD
{
    GENERATED_BODY()

    void DrawHUD() override;
};
