#pragma once

#include "GameFramework/HUD.h"
#include "Math/Box2D.h"
#include "SurvivalVitals.h"
#include "SurvivalHUD.generated.h"

class ASurvivalCharacter;
class SWidget;
class UTexture2D;
struct FSurvivalBuildPlacement;

UCLASS()
class ASurvivalHUD : public AHUD
{
    GENERATED_BODY()

public:
    void SetInventoryVisible(ASurvivalCharacter& Character, bool bVisible);
    UTexture2D* GetItemIcon(ESurvivalItem Item) const;
    FBox2f GetItemIconUV(ESurvivalItem Item) const;

protected:
    void BeginPlay() override;
    void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void DrawHUD() override;
    void DrawHotbar(const ASurvivalCharacter& Character);
    void RemoveInventoryWidget();
    void DrawBuildMenu(const ASurvivalCharacter& Character);
    void DrawPlacement(const FSurvivalBuildPlacement& Placement, const FLinearColor& Color);

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTexture2D>> ItemIcons;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> MedicineSheet;

    TSharedPtr<SWidget> InventoryWidget;
};
