#pragma once

#include "GameFramework/HUD.h"
#include "SurvivalHUD.generated.h"

class ASurvivalCharacter;
struct FSurvivalBuildPlacement;

UCLASS()
class ASurvivalHUD : public AHUD
{
    GENERATED_BODY()

    void DrawHUD() override;
    void DrawBuildMenu(const ASurvivalCharacter& Character);
    void DrawPlacement(const FSurvivalBuildPlacement& Placement, const FLinearColor& Color);
};
