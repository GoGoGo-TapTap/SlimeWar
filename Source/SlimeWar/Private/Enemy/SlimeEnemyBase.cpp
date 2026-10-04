// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enemy/SlimeEnemyBase.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/SlimeGameplayTags.h"
#include "Core/SlimeHealthComponent.h"
#include "Core/SlimeStateComponent.h"
#include "Core/SlimeWarCVars.h"
#include "Core/SlimeWarLog.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
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
	}

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(RootComponent);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Health = CreateDefaultSubobject<USlimeHealthComponent>(TEXT("Health"));
	Health->bAuthoritative = true;

	State = CreateDefaultSubobject<USlimeStateComponent>(TEXT("State"));

	GetCharacterMovement()->bOrientRotationToMovement = true;
}

void ASlimeEnemyBase::BeginPlay()
{
	Super::BeginPlay();

	if (State && TargetKind == ETargetKind::Normal)
	{
		State->AddStateTag(TAG_State_Enemy_Normal_Idle);
	}

	if (Health)
	{
		Health->OnDeath.AddDynamic(this, &ASlimeEnemyBase::HandleDeath);
	}

	ApplyStatRow(Mass);
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

void ASlimeEnemyBase::ApplyStatRow(int32 NewMass)
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

	if (Row.BodyRadius > 0.f)
	{
		GetCapsuleComponent()->SetCapsuleRadius(Row.BodyRadius);
	}

	if (BodyMesh)
	{
		if (UStaticMesh* LoadedMesh = Row.Mesh.LoadSynchronous())
		{
			BodyMesh->SetStaticMesh(LoadedMesh);
		}
	}

	if (Health)
	{
		Health->InitializeHealth(Row.MaxHealth);
	}
}

void ASlimeEnemyBase::HandleDeath()
{
	if (SlimeCVars::DebugCombatLog != 0)
	{
		UE_LOG(LogSlimeWar, Log, TEXT("ASlimeEnemyBase: %s died (kind=%d mass=%d)."),
			*GetNameSafe(this), static_cast<int32>(TargetKind), Mass);
	}

	if (State)
	{
		State->RemoveStateTag(TAG_State_Enemy_Normal_Idle);
	}

	NotifyDirectorOnDeath();

	// TODO(Phase A/B): splash placeholder and return to the object pool.
}

void ASlimeEnemyBase::NotifyDirectorOnDeath()
{
	// The director decides what a kill is worth; the enemy never touches the score system.
	if (ASlimeWarGameMode* GameMode = GetSlimeGameMode(this))
	{
		GameMode->OnEnemyKilled(TargetKind, Mass);
	}
}
