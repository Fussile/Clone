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
    if (Character->IsBuilding())
    {
        const TOptional<FSurvivalWallPlacement> Placement = Character->FindWallPlacement();
        const bool bCanBuild = Placement.IsSet() && Placement->bClear && Character->GetSupplyCount() > 0;
        const FLinearColor Color = bCanBuild ? FLinearColor(0.25f, 1.0f, 0.4f) : FLinearColor(1.0f, 0.35f, 0.25f);
        const TCHAR* Status = TEXT("Aim at level ground within 5 m");
        if (Character->GetSupplyCount() <= 0)
        {
            Status = TEXT("Need 1 supply");
        }
        else if (Placement.IsSet())
        {
            Status = bCanBuild ? TEXT("[LMB] Place wall (1 supply)") : TEXT("Wall needs clear, supported ground");
        }
        DrawText(Status, Color, CenterX + 12.0f, CenterY + 14.0f);
        DrawText(TEXT("[R] Rotate   [B] Exit building"), FLinearColor::White, 24.0f, Canvas->SizeY - 72.0f);
        if (Placement.IsSet())
        {
            DrawWallPreview(*Placement, Color);
        }
        return;
    }

    DrawText(TEXT("[B] Build wall"), FLinearColor::White, 24.0f, Canvas->SizeY - 72.0f);
    if (const ASurvivalSupplyCrate* Crate = Character->FindSupplyCrate())
    {
        const FString Prompt = Crate->HasSupplies() ? TEXT("[E] Search crate") : TEXT("Crate empty");
        DrawText(Prompt, FLinearColor::White, CenterX + 12.0f, CenterY + 14.0f);
    }
}

void ASurvivalHUD::DrawWallPreview(const FSurvivalWallPlacement& Placement, const FLinearColor& Color)
{
    FVector2D Corners[8];
    const FVector Half = FSurvivalWallPlacement::HalfExtent;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Corners); ++Index)
    {
        const FVector Offset(Index & 1 ? Half.X : -Half.X, Index & 2 ? Half.Y : -Half.Y,
            Index & 4 ? Half.Z : -Half.Z);
        const FVector Point = Placement.Center + Placement.Rotation.RotateVector(Offset);
        if (!PlayerOwner->ProjectWorldLocationToScreen(Point, Corners[Index], /*bPlayerViewportRelative=*/true))
        {
            return;
        }
    }

    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Corners); ++Index)
    {
        // Flipping one axis bit reaches the corner at the other end of an edge.
        for (int32 AxisBit : {1, 2, 4})
        {
            if (!(Index & AxisBit))
            {
                const FVector2D& Start = Corners[Index];
                const FVector2D& End = Corners[Index | AxisBit];
                DrawLine(Start.X, Start.Y, End.X, End.Y, Color, 2.0f);
            }
        }
    }
}
