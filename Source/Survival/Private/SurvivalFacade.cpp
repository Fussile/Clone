#include "SurvivalFacade.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

ASurvivalFacade::ASurvivalFacade()
{
    Panels = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("FacadePanels"));
    SetRootComponent(Panels);
    Panels->SetMobility(EComponentMobility::Static);
    Panels->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Panels->SetCanEverAffectNavigation(false);
}

void ASurvivalFacade::FitPanels(UStaticMesh* PanelMesh, double WidthCm, double DepthCm, double HeightCm,
    double GroundHeightCm, double EntranceWidthCm, const TArray<UMaterialInterface*>& Materials)
{
#if WITH_EDITOR
    Modify();
    Panels->Modify();
#endif
    Panels->ClearInstances();
    Panels->EmptyOverrideMaterials();
    Panels->SetStaticMesh(PanelMesh);
    for (int32 Slot = 0; Slot < Materials.Num(); ++Slot)
    {
        Panels->SetMaterial(Slot, Materials[Slot]);
    }
    if (!PanelMesh)
    {
        return;
    }

    const FBox Bounds = PanelMesh->GetBoundingBox();
    const FVector Size = Bounds.GetSize();
    if (Size.GetMin() <= UE_SMALL_NUMBER)
    {
        return;
    }
    const bool bWideX = Size.X >= Size.Y;
    const double ModuleWidth = FMath::Max(Size.X, Size.Y);
    const double Thickness = FMath::Min(Size.X, Size.Y);
    const int32 Rows = FMath::Max(1, FMath::RoundToInt32((HeightCm - GroundHeightCm) / Size.Z));
    const double RowHeight = (HeightCm - GroundHeightCm) / Rows;
    TArray<FTransform> Instances;
    for (int32 Face = 0; Face < 4; ++Face)
    {
        const bool bFrontOrBack = Face % 2 == 0;
        const double Span = bFrontOrBack ? WidthCm : DepthCm;
        const double Outset = ((bFrontOrBack ? DepthCm : WidthCm) + Thickness) * 0.5;
        const FRotator FaceRotation(0, Face * 90.0, 0);
        const FQuat Rotation = FRotator(0, Face * 90.0 + (bWideX ? 0 : 90), 0).Quaternion();
        auto AddBays = [&](double Left, double Right, double Bottom, double BayHeight)
        {
            const double Run = Right - Left;
            const int32 Columns = FMath::Max(1, FMath::RoundToInt32(Run / ModuleWidth));
            const double BayWidth = Run / Columns;
            const FVector Scale(bWideX ? BayWidth / Size.X : 1.0, bWideX ? 1.0 : BayWidth / Size.Y,
                BayHeight / Size.Z);
            const FVector PanelCenterOffset = Rotation.RotateVector(Scale * Bounds.GetCenter());
            for (int32 Col = 0; Col < Columns; ++Col)
            {
                const FVector Center(Left + (Col + 0.5) * BayWidth, Outset, Bottom + BayHeight * 0.5);
                Instances.Emplace(Rotation, FaceRotation.RotateVector(Center) - PanelCenterOffset, Scale);
            }
        };

        // The front faces local -Y. Leave its center open for the gameplay door.
        if (Face == 2)
        {
            AddBays(-Span * 0.5, -EntranceWidthCm * 0.5, 0, GroundHeightCm);
            AddBays(EntranceWidthCm * 0.5, Span * 0.5, 0, GroundHeightCm);
        }
        else
        {
            AddBays(-Span * 0.5, Span * 0.5, 0, GroundHeightCm);
        }
        for (int32 Row = 0; Row < Rows; ++Row)
        {
            AddBays(-Span * 0.5, Span * 0.5, GroundHeightCm + Row * RowHeight, RowHeight);
        }
    }
    Panels->AddInstances(Instances, false, false, false);
}
