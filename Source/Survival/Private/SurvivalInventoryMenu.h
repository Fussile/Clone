#pragma once

#include "Styling/SlateBrush.h"
#include "SurvivalVitals.h"
#include "Widgets/SCompoundWidget.h"

class ASurvivalCharacter;
class ASurvivalHUD;

class SSurvivalInventoryMenu : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SSurvivalInventoryMenu) {}
        SLATE_ARGUMENT(ASurvivalCharacter*, Player)
        SLATE_ARGUMENT(ASurvivalHUD*, HUD)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);
    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
    const FSlateBrush* FindIcon(ESurvivalItem Item) const;

    TWeakObjectPtr<ASurvivalCharacter> Player;
    FSlateBrush Icons[static_cast<int32>(ESurvivalItem::None)];
};
