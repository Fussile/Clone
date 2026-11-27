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
    void SetLootVisible(ASurvivalCharacter& Character, bool bVisible);
    UTexture2D* GetItemIcon(ESurvivalItem Item, int32 Model = INDEX_NONE) const;
    FBox2f GetItemIconUV(ESurvivalItem Item, int32 Model = INDEX_NONE) const;

protected:
    void BeginPlay() override;
    void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void DrawHUD() override;
    void DrawRifleScope(const ASurvivalCharacter& Character, float CenterX, float CenterY);
    void DrawHotbar(const ASurvivalCharacter& Character);
    void RemoveMenuRoot();
    void RefreshMenuRoot(const TSharedPtr<SWidget>& Focus);
    void DrawBuildMenu(const ASurvivalCharacter& Character);
    void DrawPlacement(const FSurvivalBuildPlacement& Placement, const FLinearColor& Color);

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTexture2D>> ItemIcons;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> ItemAtlas;
    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> WorkshopAtlas;
    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> FieldWorkshopAtlas;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> FirearmAtlas;
    UPROPERTY()
    TObjectPtr<UTexture2D> FieldAtlas;

    TSharedPtr<SWidget> InventoryWidget;
    TSharedPtr<SWidget> LootWidget;
    TSharedPtr<SWidget> MenuRoot;
    bool bMenuInputActive = false;
    bool bMenuBlocksMovement = false;
};
