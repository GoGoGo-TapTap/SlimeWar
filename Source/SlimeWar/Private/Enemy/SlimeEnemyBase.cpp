// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enemy/SlimeEnemyBase.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/SlimeActivityArea.h"
#include "Core/SlimeGameplayTags.h"
#include "Core/SlimeHealthComponent.h"
#include "Core/SlimeStateComponent.h"
#include "Core/SlimeWarCVars.h"
#include "Core/SlimeWarLog.h"
#include "Enemy/SlimeAIController.h"
#include "Enemy/SlimeFusionComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayFramework/SlimeRunConfig.h"
#include "GameplayFramework/SlimeWarGameMode.h"
#include "GameplayFramework/StatTableProvider.h"
#include "Kismet/GameplayStatics.h"

ASlimeEnemyBase::ASlimeEnemyBase()
{
	PrimaryActorTick.bCanEverTick = false;

	// Slimes use a static mesh, so the inherited skeletal mesh is switched off.
	if (USkeletalMeshComponent* SkeletalMesh = GetMesh())
	{
		SkeletalMesh->SetVisibility(false);
		SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(RootComponent);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DefaultBodyMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
		TEXT("/Game/Characters/Slime/SM_Slime_Base.SM_Slime_Base")));

	Health = CreateDefaultSubobject<USlimeHealthComponent>(TEXT("Health"));
	Health->bAuthoritative = true;

	State = CreateDefaultSubobject<USlimeStateComponent>(TEXT("State"));

	GetCharacterMovement()->bOrientRotationToMovement = true;

	// The weapon traces against ECC_Visibility, so the capsule must block that channel
	// explicitly (the default Pawn profile is not guaranteed to).
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

		// The player's spring arm probes with ECC_Camera, and a Pawn capsule blocks that channel by
		// default. Walking inside a slime (the player overlaps SlimeFusion on purpose) would then
		// collapse the boom and snap the camera into the character. Enemies never block the camera;
		// walls still do, because those are WorldStatic.
		Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}

	// Pawn defaults to PlacedInWorld, which means runtime spawned enemies would never get an
	// AIController and their StateTree would never start.
	AIControllerClass = ASlimeAIController::StaticClass();
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

	if (State && TargetKind == ETargetKind::Normal)
	{
		State->AddStateTag(TAG_State_Enemy_Normal_Idle);
	}

	if (Health)
	{
		Health->OnDeath.AddDynamic(this, &ASlimeEnemyBase::HandleDeath);
	}

	IgnorePlayerForMovement();

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
		const USlimeRunConfig* Config = Provider ? Provider->GetRunConfig() : nullptr;

		return Config ? Config->AIActivityRadius : 0.f;
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

	// The player definitely exists by the time anything spawns enemies at runtime.
	IgnorePlayerForMovement();
}

bool ASlimeEnemyBase::IsInsideActivityArea(float SlackCm) const
{
	return SlimeActivityArea::IsInside(GetActorLocation(), ActivityCenter, ActivityRadius, SlackCm);
}

FVector ASlimeEnemyBase::ClampToActivityArea(const FVector& WorldLocation) const
{
	return SlimeActivityArea::Clamp(WorldLocation, ActivityCenter, ActivityRadius);
}

void ASlimeEnemyBase::IgnorePlayerForMovement()
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn || PlayerPawn == this)
	{
		return;
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->IgnoreActorWhenMoving(PlayerPawn, true);
	}
}

void ASlimeEnemyBase::SetRunFrozen(bool bFrozen)
{
	if (bRunFrozen == bFrozen)
	{
		return;
	}

	bRunFrozen = bFrozen;

	// Drop the pairing first: CancelPairing notifies the partner and clears the ignore/collision
	// state, and doing it before the movement is stopped keeps the log readable.
	if (bFrozen)
	{
		if (Fusion && Fusion->IsEngaged())
		{
			Fusion->CancelPairing(ESlimeFusionCancelReason::ConditionsLost);
		}

		SetFusionTarget(nullptr);
	}

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
	if (IsAggressive())
	{
		ApplyAggroStatRow();
		return;
	}

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

FName ASlimeEnemyBase::GetAggroRowName() const
{
	const UStatTableProvider* Provider = GetGameInstance() ? GetGameInstance()->GetSubsystem<UStatTableProvider>() : nullptr;
	const USlimeRunConfig* RunConfig = Provider ? Provider->GetRunConfig() : nullptr;

	if (RunConfig && !RunConfig->AggroRowName.IsNone())
	{
		return RunConfig->AggroRowName;
	}

	return FName(TEXT("Default"));
}

void ASlimeEnemyBase::ApplyAggroStatRow()
{
	UStatTableProvider* Provider = GetGameInstance() ? GetGameInstance()->GetSubsystem<UStatTableProvider>() : nullptr;

	FSlimeAggroStatRow Row;
	if (!Provider || !Provider->GetAggroStat(Row))
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("ASlimeEnemyBase::ApplyAggroStatRow: no aggro row '%s' on %s, keeping current values."),
			*GetAggroRowName().ToString(), *GetNameSafe(this));
		return;
	}

	if (Row.MoveSpeed > 0.f)
	{
		GetCharacterMovement()->MaxWalkSpeed = Row.MoveSpeed;
	}

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
		Health->InitializeHealth(Row.MaxHealth);
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

void ASlimeEnemyBase::SetFusionTarget(ASlimeEnemyBase* InTarget)
{
	FusionTarget = InTarget;
}

void ASlimeEnemyBase::ClearAllStateTags()
{
	if (!State)
	{
		return;
	}

	static const FGameplayTag TagsToClear[] = {
		TAG_State_Enemy_Normal_Idle,
		TAG_State_Enemy_Normal_Fusing,
		TAG_State_Enemy_Normal_MassLocked,
		TAG_State_Enemy_Aggro_Chasing,
		TAG_State_Enemy_Aggro_WindingUp,
		TAG_State_Enemy_Aggro_Recovering,
		TAG_State_Enemy_Aggro_Cooling
	};

	for (const FGameplayTag& Tag : TagsToClear)
	{
		State->RemoveStateTag(Tag);
	}
}

void ASlimeEnemyBase::HandleDeath()
{
	if (SlimeCVars::DebugCombatLog != 0)
	{
		UE_LOG(LogSlimeWar, Log, TEXT("ASlimeEnemyBase: %s died (kind=%d mass=%d)."),
			*GetNameSafe(this), static_cast<int32>(TargetKind), Mass);
	}

	// Tell the partner before anything is cleared: a fusion that is still approaching or in
	// contact must be cancelled so the survivor does not keep a stale (and soon destroyed)
	// target. Only the slime that actually died is scored, once, through NotifyDirectorOnDeath.
	if (Fusion)
	{
		Fusion->NotifyPartnerDied();
	}

	FusionTarget = nullptr;
	ClearAllStateTags();

	// The AI controller listens to this and stops its StateTree before the actor goes away.
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
	Context.bHostileCore = IsAggressive();
	Context.bKilledByPlayer = !IsAggressive();
	Context.Location = GetActorLocation();
	if (const USlimeHealthComponent* HealthComponent = GetHealthComponent())
	{
		Context.Causer = HealthComponent->GetLastDamageCauser();
	}
	return Context;
}
