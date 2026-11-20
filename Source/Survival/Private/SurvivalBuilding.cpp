#include "SurvivalBuilding.h"

void FSurvivalMaterials::Add(const FSurvivalMaterials& Other)
{
    Timber += Other.Timber;
    Stone += Other.Stone;
}

void FSurvivalMaterials::Spend(const FSurvivalMaterials& Cost)
{
    Timber -= Cost.Timber;
    Stone -= Cost.Stone;
}

FSurvivalMaterials FSurvivalMaterials::MissingFrom(const FSurvivalMaterials& Inventory) const
{
    return {FMath::Max(0, Timber - Inventory.Timber), FMath::Max(0, Stone - Inventory.Stone)};
}

FString FSurvivalMaterials::ToText() const
{
    if (Timber == 0)
    {
        return Stone == 0 ? TEXT("None") : FString::Printf(TEXT("%d stone"), Stone);
    }
    if (Stone == 0)
    {
        return FString::Printf(TEXT("%d timber"), Timber);
    }
    return FString::Printf(TEXT("%d timber / %d stone"), Timber, Stone);
}

TConstArrayView<FSurvivalBuildPiece> GetSurvivalBuildPieces()
{
    const auto Box = [](double X0, double Y0, double Z0, double X1, double Y1, double Z1)
    {
        return FBox(FVector(X0, Y0, Z0), FVector(X1, Y1, Z1));
    };

    static const FSurvivalBuildPiece Pieces[] = {
        {TEXT("Timber wall"), {3, 0}, false, {Box(-50, -12, 0, 50, 12, 220)}},
        {TEXT("Half wall"), {2, 0}, false, {Box(-50, -12, 0, 50, 12, 110)}},
        {TEXT("Floor"), {2, 0}, false, {Box(-50, -50, 0, 50, 50, 18)}},
        {TEXT("Door frame"), {4, 0}, false, {
            Box(-50, -12, 0, -45, 12, 220), Box(45, -12, 0, 50, 12, 220), Box(-45, -12, 200, 45, 12, 220)}},
        {TEXT("Window wall"), {4, 0}, false, {
            Box(-50, -12, 0, 50, 12, 90), Box(-50, -12, 170, 50, 12, 220),
            Box(-50, -12, 90, -32, 12, 170), Box(32, -12, 90, 50, 12, 170)}},
        {TEXT("Stairs"), {5, 0}, false, {
            Box(-50, -50, 0, 50, -25, 25), Box(-50, -25, 0, 50, 0, 50),
            Box(-50, 0, 0, 50, 25, 75), Box(-50, 25, 0, 50, 50, 100)}},
        {TEXT("Stone wall"), {0, 5}, true, {Box(-50, -14, 0, 50, 14, 220)}},
        {TEXT("Stone foundation"), {0, 4}, true, {Box(-50, -50, 0, 50, 50, 25)}},
    };
    return MakeArrayView(Pieces);
}
