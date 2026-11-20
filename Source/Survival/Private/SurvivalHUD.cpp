#include "SurvivalHUD.h"

#include "Engine/Canvas.h"
#include "GameFramework/PlayerController.h"
#include "SurvivalCharacter.h"
#include "SurvivalSupplyCrate.h"

void ASurvivalHUD::DrawHUD()
{
    Super::DrawHUD();
    const ASurvivalCharacter* Character = PlayerOwner
        ? Cast<ASurvivalCharacter>(PlayerOwner->GetPawn()) : nullptr;
    if (!Canvas || !Character)
    {
        return;
    }

    const float CenterX = Canvas->SizeX * 0.5f;
    const float CenterY = Canvas->SizeY * 0.5f;
    DrawRect(FLinearColor::White, CenterX - 1.5f, CenterY - 1.5f, 3.0f, 3.0f);
    DrawText(FString::Printf(TEXT("Supplies: %d"), Character->GetSupplyCount()),
        FLinearColor::White, 24.0f, Canvas->SizeY - 48.0f);
    if (const ASurvivalSupplyCrate* Crate = Character->FindSupplyCrate())
    {
        const FString Prompt = Crate->HasSupplies() ? TEXT("[E] Search crate") : TEXT("Crate empty");
        DrawText(Prompt, FLinearColor::White, CenterX + 12.0f, CenterY + 14.0f);
    }
}
