#pragma once

#include "GameFramework/HUD.h"
#include "SurvivalHUD.generated.h"

struct FSurvivalWallPlacement;

UCLASS()
class ASurvivalHUD : public AHUD
{
    GENERATED_BODY()

    void DrawHUD() override;
    void DrawWallPreview(const FSurvivalWallPlacement& Placement, const FLinearColor& Color);
};
