#include "SurvivalArsenal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurvivalHeldFirearmActions, "Survival.Weapons.HeldFirearmActions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSurvivalHeldFirearmActions::RunTest(const FString& Parameters)
{
    for (const auto& Weapon : Arsenal)
    {
        const FString Model(Weapon.Asset);
        const bool bAutomatic = Model == TEXT("AK12") || Model == TEXT("HK416") ||
            Model == TEXT("MP7") || Model == TEXT("Scorpion");
        TestEqual(Model + TEXT(" default automatic fire"), Weapon.AutomaticInterval > 0.f, bAutomatic);
        if (!bAutomatic && Model != TEXT("Python357")) continue;

        const FString Folder = FString::Printf(TEXT("/Game/Art/Crossroads/Arsenal/%s/Actions/"), Weapon.Asset);
        const auto* Rig = LoadObject<USkeletalMesh>(nullptr, *(Folder + TEXT("Rig0.Rig0")));
        if (!TestNotNull(Model + TEXT(" held rig"), Rig)) continue;
        TSet<const UAnimSequence*> Distinct;
        for (const TCHAR* Role : {TEXT("Idle"), TEXT("Walk"), TEXT("Fire"), TEXT("Reload")})
        {
            const FString Path = Folder + Role + TEXT(".") + Role;
            const auto* Clip = LoadObject<UAnimSequence>(nullptr, *Path);
            if (!TestNotNull(Path, Clip)) continue;
            TestTrue(Path + TEXT(" animates the held skeleton"), Clip->GetSkeleton() == Rig->GetSkeleton());
            TestTrue(Path + TEXT(" contains a timed action"), Clip->GetPlayLength() > 0.f);
            Distinct.Add(Clip);
        }
        TestEqual(Model + TEXT(" independent actions"), Distinct.Num(), 4);
    }
    return true;
}
#endif
