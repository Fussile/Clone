#include "CrossroadsWorldBake.h"

#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"

namespace
{
struct FGunTriangle
{
    FVector Corners[3];
    int32 Section;
};

int32 PartNumber(FName ImportedName, const UMaterialInterface* Material, bool bScrapRifle)
{
    FString Label = ImportedName.ToString();
    if (!Label.Contains(TEXT("GunPart")) && Material) Label = Material->GetName();
    // The scrap rifle has a raised sling in its Other material. Its arc crosses the optic station.
    if (bScrapRifle && Label.Contains(TEXT("_Other"))) return INDEX_NONE;
    const int32 Prefix = Label.Find(TEXT("GunPart"));
    return Prefix != INDEX_NONE && Label.Len() > Prefix + 7 ? Label[Prefix + 7] - '0' : INDEX_NONE;
}

// Intersect actual triangles with a plane across the weapon. Long, sparsely
// tessellated barrels have few vertices near their tips, but still intersect it.
FBox Slice(const TArray<FGunTriangle>& Triangles, double X, bool bIncludeMagazine)
{
    FBox Bounds(ForceInit);
    for (const auto& Triangle : Triangles)
    {
        if (Triangle.Section == 3 && !bIncludeMagazine) continue;
        for (int32 Edge = 0; Edge < 3; ++Edge)
        {
            const FVector A = Triangle.Corners[Edge], B = Triangle.Corners[(Edge + 1) % 3];
            if (A.X == X) Bounds += A;
            if ((A.X < X && B.X > X) || (B.X < X && A.X > X))
                Bounds += FMath::Lerp(A, B, (X - A.X) / (B.X - A.X));
        }
    }
    return Bounds;
}

FVector MajorAxis(const FVector& Size)
{
    if (Size.X >= Size.Y && Size.X >= Size.Z) return FVector::ForwardVector;
    return Size.Y >= Size.Z ? FVector::RightVector : FVector::UpVector;
}
}

bool UCrossroadsWorldBake::PrepareWeaponMounts(UObject* WeaponMesh, bool bPistol)
{
    auto* Static = Cast<UStaticMesh>(WeaponMesh);
    auto* Skinned = Cast<USkeletalMesh>(WeaponMesh);
    const FMeshDescription* Description = Static ? Static->GetMeshDescription(0) :
        Skinned ? Skinned->GetMeshDescription(0) : nullptr;
    if (!Description) return false;
    const FStaticMeshConstAttributes Attributes(*Description);
    const auto SlotNames = Attributes.GetPolygonGroupMaterialSlotNames();
    const auto Positions = Attributes.GetVertexPositions();
    TArray<FGunTriangle> Triangles;
    FBox Sections[] = {FBox(ForceInit), FBox(ForceInit), FBox(ForceInit), FBox(ForceInit)};
    FBox Entire(ForceInit);
    const bool bScrapRifle = WeaponMesh->GetPathName().Contains(TEXT("/ScrapRifle/"));
    for (const FPolygonGroupID Group : Description->PolygonGroups().GetElementIDs())
    {
        const FName Name = SlotNames[Group];
        const UMaterialInterface* Material = nullptr;
        if (Static)
        {
            const auto* Slot = Static->GetStaticMaterials().FindByPredicate([Name](const FStaticMaterial& M) {
                return M.MaterialSlotName == Name || M.ImportedMaterialSlotName == Name;
            });
            if (Slot) Material = Slot->MaterialInterface;
        }
        else
        {
            const auto* Slot = Skinned->GetMaterials().FindByPredicate([Name](const FSkeletalMaterial& M) {
                return M.MaterialSlotName == Name || M.ImportedMaterialSlotName == Name;
            });
            if (Slot) Material = Slot->MaterialInterface;
        }
        const int32 Section = PartNumber(Name, Material, bScrapRifle);
        if (Section < 0 || Section >= 4) continue; // Excludes arms and removable factory optic.
        for (const FTriangleID Id : Description->GetPolygonGroupTriangles(Group))
        {
            auto& T = Triangles.AddDefaulted_GetRef();
            T.Section = Section;
            const auto Verts = Description->GetTriangleVertices(Id);
            for (int32 I = 0; I < 3; ++I)
            {
                T.Corners[I] = FVector(Positions[Verts[I]]);
                Sections[Section] += T.Corners[I];
                Entire += T.Corners[I];
            }
        }
    }
    if (!Entire.IsValid) return false;
    FVector Forward, Up;
    FName Bone;
    FTransform Parent = FTransform::Identity;
    if (Skinned)
    {
        const auto& Ref = Skinned->GetRefSkeleton();
        const bool bSniper = Ref.FindBoneIndex(TEXT("ViewBreech")) != INDEX_NONE;
        const bool bSidearm = Ref.FindBoneIndex(TEXT("RearSight")) != INDEX_NONE;
        const int32 Rear = Ref.FindBoneIndex(bSniper ? TEXT("ViewBreech") : bSidearm ? TEXT("RearSight") : TEXT("GunRear"));
        const int32 Front = Ref.FindBoneIndex(bSniper ? TEXT("ViewMuzzle") : bSidearm ? TEXT("FrontSight") : TEXT("GunFront"));
        const int32 Top = Ref.FindBoneIndex(bSniper ? TEXT("ViewScopeEye") : bSidearm ? TEXT("SightUp") : TEXT("GunTop"));
        if (Rear == INDEX_NONE || Front == INDEX_NONE || Top == INDEX_NONE) return false;
        const FVector Start = Skinned->GetComposedRefPoseMatrix(Rear).GetOrigin();
        Forward = Skinned->GetComposedRefPoseMatrix(Front).GetOrigin() - Start;
        Up = Skinned->GetComposedRefPoseMatrix(Top).GetOrigin() - Start;
        const int32 Receiver = bSniper ? Ref.FindBoneIndex(TEXT("Sniper_base")) : Ref.GetParentIndex(Rear);
        if (Receiver == INDEX_NONE) return false;
        Bone = Ref.GetBoneName(Receiver);
        Parent = FTransform(Skinned->GetComposedRefPoseMatrix(Receiver));
    }
    else
    {
        Forward = MajorAxis(Entire.GetSize());
        const FVector Back = Sections[0].IsValid ? Sections[0].GetCenter() : Entire.GetCenter();
        const FVector Nose = Sections[2].IsValid ? Sections[2].GetCenter() : Entire.GetCenter();
        if (FVector::DotProduct(Forward, Nose - Back) < 0) Forward *= -1;
        Up = MajorAxis(Entire.GetSize() * (FVector::OneVector - Forward.GetAbs()));
        const FVector Lower = !bPistol && Sections[3].IsValid ? Sections[3].GetCenter() : Back;
        const FVector Upper = bPistol || !Sections[1].IsValid ? Nose : Sections[1].GetCenter();
        if (FVector::DotProduct(Up, Upper - Lower) < 0) Up *= -1;
    }
    if (FVector::CrossProduct(Forward.GetSafeNormal(), Up.GetSafeNormal()).IsNearlyZero()) return false;
    const FQuat Frame = FRotationMatrix::MakeFromXZ(Forward, Up).ToQuat();
    FBox Body(ForceInit);
    for (auto& T : Triangles)
        for (auto& P : T.Corners)
        {
            P = Frame.UnrotateVector(P);
            if (T.Section != 3) Body += P;
        }
    if (!Body.IsValid || Body.GetSize().X <= UE_SMALL_NUMBER) return false;

    enum class EFace { Middle, Top, Bottom, Left, Right };
    struct FMount { const TCHAR* Name; double Fraction; EFace Face; };
    const FMount Mounts[] = {
        {TEXT("MountRear"), .006, EFace::Middle}, {TEXT("MountMuzzle"), .994, EFace::Middle},
        {TEXT("MountOptic"), bPistol ? .24 : .46, EFace::Top}, {TEXT("MountLaser"), .73, EFace::Right},
        {TEXT("MountLight"), .77, EFace::Left}, {TEXT("MountUnderbarrel"), .69, EFace::Bottom},
        {TEXT("MountMagazine"), bPistol ? .22 : .52, EFace::Bottom}, {TEXT("MountSling"), .27, EFace::Left},
        {TEXT("MountAction"), .45, EFace::Right}, {TEXT("MountStock"), .12, EFace::Middle},
    };
    TArray<FTransform> Fits;
    for (const auto& Mount : Mounts)
    {
        const double X = FMath::Lerp(Body.Min.X, Body.Max.X, Mount.Fraction);
        FBox Section = Slice(Triangles, X, FCString::Strcmp(Mount.Name, TEXT("MountMagazine")) == 0);
        if (!Section.IsValid) return false;
        FVector Point = Section.GetCenter();
        if (Mount.Face == EFace::Top) Point.Z = Section.Max.Z;
        if (Mount.Face == EFace::Bottom) Point.Z = Section.Min.Z;
        if (Mount.Face == EFace::Left) Point.Y = Section.Min.Y;
        if (Mount.Face == EFace::Right) Point.Y = Section.Max.Y;
        Fits.Add(FTransform(Frame, Frame.RotateVector(Point)).GetRelativeTransform(Parent));
    }
    WeaponMesh->Modify();
    for (int32 I = 0; I < UE_ARRAY_COUNT(Mounts); ++I)
    {
        const FName Name(Mounts[I].Name);
        const auto& Fit = Fits[I];
        if (Static)
        {
            auto* Socket = Static->FindSocket(Name);
            if (!Socket)
            {
                Socket = NewObject<UStaticMeshSocket>(Static);
                Socket->SocketName = Name;
                Static->AddSocket(Socket);
            }
            Socket->RelativeLocation = Fit.GetLocation();
            Socket->RelativeRotation = Fit.Rotator();
        }
        else
        {
            auto& Sockets = Skinned->GetMeshOnlySocketList();
            const auto* Existing = Sockets.FindByPredicate([Name](const auto& S) { return S && S->SocketName == Name; });
            auto* Socket = Existing ? Existing->Get() : NewObject<USkeletalMeshSocket>(Skinned);
            if (!Existing) Sockets.Add(Socket);
            Socket->SocketName = Name;
            Socket->BoneName = Bone;
            Socket->RelativeLocation = Fit.GetLocation();
            Socket->RelativeRotation = Fit.Rotator();
            Socket->RelativeScale = Fit.GetScale3D();
        }
    }
    WeaponMesh->PostEditChange();
    WeaponMesh->MarkPackageDirty();
    return true;
}
