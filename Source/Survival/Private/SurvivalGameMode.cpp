#include "SurvivalGameMode.h"
#include "SurvivalCharacter.h"
#include "SurvivalHUD.h"

ASurvivalGameMode::ASurvivalGameMode()
{
    DefaultPawnClass = ASurvivalCharacter::StaticClass();
    HUDClass = ASurvivalHUD::StaticClass();
}
