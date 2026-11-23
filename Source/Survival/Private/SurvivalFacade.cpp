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

void ASurvivalFacade::FitPanels(UStaticMesh* PanelMesh, double WidthCm, double DepthCm, double HeightCm)
{
#if WITH_EDITOR
    Modify();
    Panels->Modify();
#endif
    Panels->ClearInstances();
    Panels->SetStaticMesh(PanelMesh);
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
    const int32 Rows = FMath::Max(1, FMath::RoundToInt32(HeightCm / Size.Z));
    const double RowHeight = HeightCm / Rows;
    TArray<FTransform> Instances;
    for (int32 Face = 0; Face < 4; ++Face)
    {
        const bool bFrontOrBack = Face % 2 == 0;
        const double Span = bFrontOrBack ? WidthCm : DepthCm;
        const double Outset = ((bFrontOrBack ? DepthCm : WidthCm) + Thickness) * 0.5;
        const int32 Columns = FMath::Max(1, FMath::RoundToInt32(Span / ModuleWidth));
        const double BayWidth = Span / Columns;
        const FVector Scale(bWideX ? BayWidth / Size.X : 1.0, bWideX ? 1.0 : BayWidth / Size.Y,
            RowHeight / Size.Z);
        const FRotator FaceRotation(0, Face * 90.0, 0);
        const FQuat Rotation = FRotator(0, Face * 90.0 + (bWideX ? 0 : 90), 0).Quaternion();
        const FVector PanelCenterOffset = Rotation.RotateVector(Scale * Bounds.GetCenter());
        for (int32 Row = 0; Row < Rows; ++Row)
        {
            for (int32 Col = 0; Col < Columns; ++Col)
            {
                const FVector Center((Col + 0.5) * BayWidth - Span * 0.5, Outset, (Row + 0.5) * RowHeight);
                Instances.Emplace(Rotation, FaceRotation.RotateVector(Center) - PanelCenterOffset, Scale);
            }
        }
    }
    Panels->AddInstances(Instances, false, false, false);
}
