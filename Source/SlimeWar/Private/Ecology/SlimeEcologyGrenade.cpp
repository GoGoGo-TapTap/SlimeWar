// Copyright Epic Games, Inc. All Rights Reserved.

#include "Ecology/SlimeEcologyGrenade.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/SlimeWarLog.h"
#include "DrawDebugHelpers.h"
#include "Ecology/SlimeBodyInteractionComponent.h"
#include "Ecology/SlimeEcologyEnemy.h"
#include "Ecology/SlimeEcologyFusionComponent.h"
#include "Ecology/SlimeEcologyRules.h"
#include "Ecology/SlimeEcologySubsystem.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

ASlimeEcologyGrenade::ASlimeEcologyGrenade()
{
	PrimaryActorTick.bCanEverTick = true;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(12.f);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->OnComponentHit.AddDynamic(this, &ASlimeEcologyGrenade::HandleHit);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
		Mesh->SetRelativeScale3D(FVector(0.18f));
	}

	Projectile = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Projectile"));
	Projectile->UpdatedComponent = Collision;
	Projectile->ProjectileGravityScale = 1.f;
	Projectile->bRotationFollowsVelocity = true;
	Projectile->bShouldBounce = false;
}

void ASlimeEcologyGrenade::BeginPlay()
{
	Super::BeginPlay();
	LifeRemaining = USlimeEcologyRules::GetFor(this)->GrenadeFlightTime;
}

const TCHAR* ASlimeEcologyGrenade::TypeName(ESlimeGrenadeType Type)
{
	return Type == ESlimeGrenadeType::Assimilation ? TEXT("Assimilation") : TEXT("Tornado");
}

void ASlimeEcologyGrenade::Launch(
	ESlimeGrenadeType InType,
	ESlimeFaction InFaction,
	const FVector& Direction)
{
	GrenadeType = InType;
	Faction = InFaction;
	const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this);
	if (Projectile && Rules)
	{
		Projectile->Velocity =
			Direction.GetSafeNormal() * Rules->GrenadeThrowSpeed
			+ FVector::UpVector * Rules->GrenadeThrowLift;
		Projectile->ProjectileGravityScale =
			Rules->GrenadeGravity / FMath::Max(1.f, GetWorld()->GetGravityZ() * -1.f);
	}
	LifeRemaining = Rules ? Rules->GrenadeFlightTime : 2.f;
}

void ASlimeEcologyGrenade::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bTornadoActive)
	{
		TickTornado(DeltaSeconds);
		return;
	}

	if (!bDetonated)
	{
		LifeRemaining -= DeltaSeconds;
		if (LifeRemaining <= 0.f)
		{
			Detonate();
		}
	}
}

void ASlimeEcologyGrenade::HandleHit(
	UPrimitiveComponent*,
	AActor*,
	UPrimitiveComponent*,
	FVector,
	const FHitResult&)
{
	if (!bDetonated)
	{
		Detonate();
	}
}

void ASlimeEcologyGrenade::Detonate()
{
	if (bDetonated)
	{
		return;
	}
	bDetonated = true;
	if (GrenadeType == ESlimeGrenadeType::Assimilation)
	{
		ApplyAssimilation();
		Destroy();
	}
	else
	{
		StartTornado();
	}
}

void ASlimeEcologyGrenade::ApplyAssimilation()
{
	const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this);
	UWorld* World = GetWorld();
	USlimeEcologySubsystem* Ecology = World
		? World->GetSubsystem<USlimeEcologySubsystem>()
		: nullptr;
	if (!Rules || !Ecology)
	{
		return;
	}

	TArray<ASlimeEcologyEnemy*> Neighbours;
	Ecology->QueryAgentsAroundLocation(
		GetActorLocation(),
		Rules->AssimilationRadius,
		Neighbours);
	for (ASlimeEcologyEnemy* Enemy : Neighbours)
	{
		if (Enemy)
		{
			Enemy->ApplyAssimilation(Faction);
		}
	}
	UE_LOG(LogSlimeWar, Log,
		TEXT("GrenadeAssimilation: faction %d affected %d slime(s) at %s."),
		static_cast<int32>(Faction),
		Neighbours.Num(),
		*GetActorLocation().ToCompactString());
}

void ASlimeEcologyGrenade::StartTornado()
{
	bTornadoActive = true;
	TornadoCentre = GetActorLocation();
	TornadoRemaining = USlimeEcologyRules::GetFor(this)->TornadoDuration;
	if (Projectile)
	{
		Projectile->StopMovementImmediately();
		Projectile->Deactivate();
	}
	if (Collision)
	{
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (Mesh)
	{
		Mesh->SetVisibility(false);
	}
}

void ASlimeEcologyGrenade::TickTornado(float DeltaSeconds)
{
	const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this);
	UWorld* World = GetWorld();
	USlimeEcologySubsystem* Ecology = World
		? World->GetSubsystem<USlimeEcologySubsystem>()
		: nullptr;
	if (!Rules || !Ecology)
	{
		Destroy();
		return;
	}

	TArray<ASlimeEcologyEnemy*> Neighbours;
	Ecology->QueryAgentsAroundLocation(TornadoCentre, Rules->TornadoRadius, Neighbours);
	for (ASlimeEcologyEnemy* Enemy : Neighbours)
	{
		USlimeBodyInteractionComponent* Body =
			Enemy ? Enemy->GetBodyInteraction() : nullptr;
		if (!Enemy || !Body)
		{
			continue;
		}

		FCollisionQueryParams SightParams(
			SCENE_QUERY_STAT(SlimeTornadoSight),
			false,
			this);
		SightParams.AddIgnoredActor(Enemy);
		FHitResult SightHit;
		if (World->LineTraceSingleByChannel(
			SightHit,
			TornadoCentre + FVector(0.f, 0.f, 30.f),
			Enemy->GetActorLocation(),
			ECC_WorldStatic,
			SightParams))
		{
			continue;
		}

		FVector ToCentre = TornadoCentre - Enemy->GetActorLocation();
		ToCentre.Z = 0.f;
		const float Distance = ToCentre.Size2D();
		if (Distance <= Rules->TornadoStopRadius)
		{
			Body->SetExternalVelocity(FVector::ZeroVector, this);
		}
		else
		{
			const float CurrentSpeed = Body->GetExternalVelocity().Size2D();
			const float NewSpeed = FMath::Min(
				Rules->TornadoMaxSpeed,
				CurrentSpeed + Rules->TornadoAcceleration * DeltaSeconds);
			Body->SetExternalVelocity(
				ToCentre.GetSafeNormal2D() * NewSpeed,
				this);
		}

		if (USlimeEcologyFusionComponent* Fusion = Enemy->GetEcologyFusionComponent())
		{
			Fusion->SetTemporaryContactReady(Rules->TornadoFusionReadyTime);
		}
	}

	TornadoRemaining -= DeltaSeconds;
	if (TornadoRemaining <= 0.f)
	{
		Destroy();
	}
}
