#include "SurvivalPlayerController.h"

#include "Engine/World.h"
#include "SurvivalGameMode.h"

void ASurvivalPlayerController::SetTimeOfDay(float Hour)
{
#if !UE_BUILD_SHIPPING
    ASurvivalGameMode* Mode = GetWorld()->GetAuthGameMode<ASurvivalGameMode>();
    if (Mode && Mode->SetTimeOfDay(Hour))
    {
        const int32 Minute = Mode->GetWorldMinutes();
        ClientMessage(FString::Printf(TEXT("Day %d, %02d:%02d"), Minute / 1440 + 1, (Minute / 60) % 24, Minute % 60));
    }
    else
    {
        ClientMessage(TEXT("SetTimeOfDay requires a host and an hour from 0 up to 24, for example SetTimeOfDay 21.5."));
    }
#endif
}
