#include "SurvivalQuarantineMine.h"
#include "SurvivalCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"

ASurvivalQuarantineMine::ASurvivalQuarantineMine()
{
    bReplicates = true;
    bNetLoadOnClient = true;
    auto* Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("PlayerTrigger"));
    SetRootComponent(Trigger);
    Trigger->InitSphereRadius(85.f);
    Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
    Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    auto* Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HazardMarker"));
    Marker->SetupAttachment(Trigger);
    Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ASurvivalQuarantineMine::NotifyActorBeginOverlap(AActor* OtherActor)
{
    Super::NotifyActorBeginOverlap(OtherActor);
    if (HasAuthority() && !bSpent)
    {
        if (ASurvivalCharacter* Player = Cast<ASurvivalCharacter>(OtherActor))
        {
            bSpent = true;
            Player->TakeSurvivalDamage(45);
            Destroy();
        }
    }
}
