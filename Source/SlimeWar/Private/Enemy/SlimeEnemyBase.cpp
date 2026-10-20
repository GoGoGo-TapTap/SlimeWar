// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enemy/SlimeEnemyBase.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/SlimeActivityArea.h"
#include "Core/SlimeHealthComponent.h"
#include "Core/SlimeWarCVars.h"
#include "Core/SlimeWarLog.h"
#include "Ecology/SlimeEcologyAIController.h"
#include "Ecology/SlimeEcologyRules.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayFramework/SlimeRunConfig.h"
#include "GameplayFramework/SlimeWarGameMode.h"
#include "GameplayFramework/StatTableProvider.h"

ASlimeEnemyBase::ASlimeEnemyBase()
{
	PrimaryActorTick.bCanEverTick = false;

	// Slimes use a static mesh, so the inherited skeletal mesh is switched off.
	if (USkeletalMeshComponent* SkeletalMesh = GetMesh())
	{
		SkeletalMesh->SetVisibility(false);
		SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SkeletalMesh->SetReceivesDecals(false);
	}

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(RootComponent);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetReceivesDecals(false);
	DefaultBodyMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
		TEXT("/Game/Characters/Slime/SM_Slime_Base.SM_Slime_Base")));

	Health = CreateDefaultSubobject<USlimeHealthComponent>(TEXT("Health"));
	Health->bAuthoritative = true;

	GetCharacterMovement()->bOrientRotationToMovement = true;

	// The weapon traces against ECC_Visibility, so the capsule must block that channel
	// explicitly (the default Pawn profile is not guaranteed to).
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetReceivesDecals(false);
		Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

		// The player's spring arm probes with ECC_Camera, and a Pawn capsule blocks that channel by
		// default. Walking inside a slime (the player overlaps SlimeFusion on purpose) would then
		// collapse the boom and snap the camera into the character. Enemies never block the camera;
		// walls still do, because those are WorldStatic.
		Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}

	// Concrete ecology bodies override this with ASlimeEcologyAIController.
	AIControllerClass = ASlimeEcologyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void ASlimeEnemyBase::BeginPlay()
{
	Super::BeginPlay();

	// Placed-in-level actors have no spawner to call InitializeFromSpawn.
	if (ActivityCenter.IsNearlyZero())
	{
		ActivityCenter = GetActorLocation();
	}

	if (Health)
	{
		Health->OnDeath.AddDynamic(this, &ASlimeEnemyBase::HandleDeath);
	}

	ApplyStatRow(Mass);
}

namespace
{
	/**
	 * Global fallback radius for slimes that were not placed by a spawn point (debug spawns,
	 * hand-placed actors). Keeps "stay inside your point" true for them too.
	 */
	float GetConfiguredActivityRadius(const AActor* Context)
	{
		const UGameInstance* GameInstance = Context ? Context->GetGameInstance() : nullptr;
		const UStatTableProvider* Provider =
			GameInstance ? GameInstance->GetSubsystem<UStatTableProvider>() : nullptr;
		const USlimeEcologyRules* Rules = Provider ? Provider->GetEcologyRules() : nullptr;
		return Rules ? Rules->WanderRadiusMax : 0.f;
	}
}

void ASlimeEnemyBase::InitializeFromSpawn(
	int32 InPointId, const FVector& InActivityCenter, float InActivityRadius)
{
	PointId = InPointId;
	ActivityCenter = InActivityCenter;

	// The spawn point hands over its effective radius (per-point value, else the global one).
	// Anything else falls back to the global value here, so those slimes are constrained too.
	ActivityRadius = InActivityRadius > 0.f
		? InActivityRadius
		: GetConfiguredActivityRadius(this);

}

bool ASlimeEnemyBase::IsInsideActivityArea(float SlackCm) const
{
	return SlimeActivityArea::IsInside(GetActorLocation(), ActivityCenter, ActivityRadius, SlackCm);
}

FVector ASlimeEnemyBase::ClampToActivityArea(const FVector& WorldLocation) const
{
	return SlimeActivityArea::Clamp(WorldLocation, ActivityCenter, ActivityRadius);
}

void ASlimeEnemyBase::SetRunFrozen(bool bFrozen)
{
	if (bRunFrozen == bFrozen)
	{
		return;
	}

	bRunFrozen = bFrozen;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		if (bFrozen)
		{
			Movement->StopMovementImmediately();
			Movement->DisableMovement();
		}
		else
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
	}

	// The StateTree ticks through the AI controller, so freezing the controller is what actually
	// stops chasing, attacking and fusing. Both are restored together.
	if (AController* OwningController = GetController())
	{
		OwningController->SetActorTickEnabled(!bFrozen);
	}

	SetActorTickEnabled(!bFrozen);

	if (SlimeCVars::DebugCombatLog != 0)
	{
		UE_LOG(LogSlimeWar, Log, TEXT("ASlimeEnemyBase: %s run %s."),
			*GetNameSafe(this), bFrozen ? TEXT("frozen") : TEXT("unfrozen"));
	}
}

float ASlimeEnemyBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	// No ASC here: USlimeHealthComponent is the authoritative health for enemies.
	if (Health)
	{
		Health->ApplyDamage(DamageAmount, DamageCauser);
	}

	return Applied;
}

void ASlimeEnemyBase::ApplyStatRow(int32 NewMass, float HealthFraction)
{
	Mass = FMath::Max(1, NewMass);

	UStatTableProvider* Provider = GetGameInstance() ? GetGameInstance()->GetSubsystem<UStatTableProvider>() : nullptr;

	FSlimeStatRow Row;
	if (!Provider || !Provider->GetSlimeStat(Mass, Row))
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("ASlimeEnemyBase::ApplyStatRow: no stat row for mass %d on %s, keeping current values."),
			Mass, *GetNameSafe(this));
		return;
	}

	Mass = Row.Mass;
	GetCharacterMovement()->MaxWalkSpeed = Row.MoveSpeed;

	if (BodyMesh)
	{
		const TSoftObjectPtr<UStaticMesh>& MeshToLoad =
			Row.Mesh.IsNull() ? DefaultBodyMesh : Row.Mesh;
		if (UStaticMesh* LoadedMesh = MeshToLoad.LoadSynchronous())
		{
			BodyMesh->SetStaticMesh(LoadedMesh);
		}
	}

	if (Row.BodyRadius > 0.f)
	{
		ApplyBodyRadius(Row.BodyRadius);
	}

	if (Health)
	{
		Health->InitializeHealth(Row.MaxHealth, HealthFraction);
	}

	OnBodyStatsApplied(Mass, Row.BodyRadius);
}

void ASlimeEnemyBase::ApplyBodyRadius(float Radius)
{
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!Capsule || Radius <= 0.f)
	{
		return;
	}

	// Spawn slots place the capsule bottom 2 cm above the ground. Preserve that foot position when
	// the capsule changes from the ACharacter default to the mass-specific sphere.
	const float OldHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector OldLocation = GetActorLocation();
	const float FootZ = OldLocation.Z - OldHalfHeight;

	// Set both dimensions explicitly: SetCapsuleRadius alone clamps half height to the old value and
	// leaves a tall cylinder whose pivot is far above the visual ball.
	Capsule->SetCapsuleSize(Radius, Radius, /*bUpdateOverlaps=*/true);

	const float NewHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	SetActorLocation(
		FVector(OldLocation.X, OldLocation.Y, FootZ + NewHalfHeight),
		/*bSweep=*/false,
		nullptr,
		ETeleportType::TeleportPhysics);

	FitBodyMeshToBodyRadius(Radius);
}

void ASlimeEnemyBase::FitBodyMeshToBodyRadius(float Radius)
{
	if (!BodyMesh || Radius <= 0.f)
	{
		return;
	}

	const UStaticMesh* StaticMesh = BodyMesh->GetStaticMesh();
	if (!StaticMesh)
	{
		return;
	}

	const FBoxSphereBounds MeshBounds = StaticMesh->GetBounds();
	const float HorizontalExtent =
		FMath::Max(MeshBounds.BoxExtent.X, MeshBounds.BoxExtent.Y);

	if (HorizontalExtent <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("ASlimeEnemyBase::FitBodyMeshToBodyRadius: %s has no usable horizontal bounds."),
			*GetNameSafe(StaticMesh));
		return;
	}

	// Match the visual half width to BodyRadius. The Z offset then puts the visual bounds bottom
	// on the capsule bottom, regardless of the mesh pivot chosen by the artist/importer.
	const float VisualScale = Radius / HorizontalExtent;
	const float VisualBottomFromPivot =
		(MeshBounds.Origin.Z - MeshBounds.BoxExtent.Z) * VisualScale;

	BodyMesh->SetRelativeScale3D(FVector(VisualScale));
	BodyMesh->SetRelativeLocation(FVector(
		-MeshBounds.Origin.X * VisualScale,
		-MeshBounds.Origin.Y * VisualScale,
		-Radius - VisualBottomFromPivot));
}

void ASlimeEnemyBase::OnBodyStatsApplied(int32 NewMass, float BodyRadius)
{
}

void ASlimeEnemyBase::HandleDeath()
{
	if (SlimeCVars::DebugCombatLog != 0)
	{
		UE_LOG(LogSlimeWar, Log, TEXT("ASlimeEnemyBase: %s died (mass=%d)."),
			*GetNameSafe(this), Mass);
	}

	OnEnemyDied.Broadcast(this);

	NotifyDirectorOnDeath();

	// Stop participating in the world immediately: no movement, no collision, no more damage.
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	SetActorEnableCollision(false);

	// TODO(Phase C, PC-08): return to the object pool instead of destroying.
	Destroy();
}

void ASlimeEnemyBase::NotifyDirectorOnDeath()
{
	// The director decides what a kill is worth; the enemy never touches the score system.
	if (ASlimeWarGameMode* GameMode = GetSlimeGameMode(this))
	{
		GameMode->OnEnemyKilled(BuildKillContext());
	}
}

FSlimeKillContext ASlimeEnemyBase::BuildKillContext() const
{
	FSlimeKillContext Context;
	Context.Mass = Mass;
	Context.BodyScoreCredit = Mass * 4;
	Context.bHostileCore = false;
	Context.bKilledByPlayer = false;
	Context.Location = GetActorLocation();
	if (const USlimeHealthComponent* HealthComponent = GetHealthComponent())
	{
		Context.Causer = HealthComponent->GetLastDamageCauser();
	}
	return Context;
}
