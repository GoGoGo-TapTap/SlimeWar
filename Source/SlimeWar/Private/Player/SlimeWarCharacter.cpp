// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/SlimeWarCharacter.h"

#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Core/SlimeGameplayTags.h"
#include "Core/SlimeHealthComponent.h"
#include "Core/SlimeWarCVars.h"
#include "Core/SlimeWarLog.h"
#include "DrawDebugHelpers.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameplayFramework/SlimeCombatSubsystem.h"
#include "GameplayFramework/SlimeRunConfig.h"
#include "GameplayFramework/SlimeWarGameMode.h"
#include "GameplayFramework/StatTableProvider.h"
#include "InputActionValue.h"
#include "Player/Abilities/GA_Fire.h"
#include "Player/Abilities/GA_HitProtection.h"
#include "Player/Abilities/GA_Reload.h"
#include "Player/SlimeAbilityInputID.h"
#include "Player/SlimePlayerAttributeSet.h"
#include "Player/SlimeWeaponComponent.h"

ASlimeWarCharacter::ASlimeWarCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Capsule stays at the template size; Phase A only moves the tuning values into data.
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Full GAS on the player (plan: layered GAS).
	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	Attributes = CreateDefaultSubobject<USlimePlayerAttributeSet>(TEXT("Attributes"));

	// Proxy health: mirrored from the AttributeSet, never authoritative.
	Health = CreateDefaultSubobject<USlimeHealthComponent>(TEXT("Health"));
	Health->bAuthoritative = false;

	Weapon = CreateDefaultSubobject<USlimeWeaponComponent>(TEXT("Weapon"));
}

UAbilitySystemComponent* ASlimeWarCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void ASlimeWarCharacter::BeginPlay()
{
	Super::BeginPlay();

	InitAbilitySystem();
	ApplyRunConfig();
}

void ASlimeWarCharacter::InitAbilitySystem()
{
	if (!AbilitySystem)
	{
		return;
	}

	AbilitySystem->InitAbilityActorInfo(this, this);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// Mirror every health change instead of keeping a second copy of the value.
	AbilitySystem->GetGameplayAttributeValueChangeDelegate(USlimePlayerAttributeSet::GetHealthAttribute())
		.AddWeakLambda(this, [this](const FOnAttributeChangeData&) { SyncHealthMirror(); });

	AbilitySystem->GetGameplayAttributeValueChangeDelegate(USlimePlayerAttributeSet::GetMaxHealthAttribute())
		.AddWeakLambda(this, [this](const FOnAttributeChangeData&) { SyncHealthMirror(); });

	if (Health)
	{
		Health->OnDeath.AddDynamic(this, &ASlimeWarCharacter::HandleMirrorDeath);
	}

	// Phase A player state machine: controllable from BeginPlay. Deploying / Result arrive
	// with Phase D; the abilities already block on those tags (PA-15).
	AbilitySystem->AddLooseGameplayTag(TAG_State_Player_Controllable);

	// Input ids line up with EAbilityInputID so AbilityLocalInputPressed reaches them.
	AbilitySystem->GiveAbility(FGameplayAbilitySpec(
		UGA_Fire::StaticClass(), 1, static_cast<int32>(EAbilityInputID::Fire)));
	AbilitySystem->GiveAbility(FGameplayAbilitySpec(
		UGA_Reload::StaticClass(), 1, static_cast<int32>(EAbilityInputID::Reload)));

	// Hit protection is triggered by code, not by an input.
	AbilitySystem->GiveAbility(FGameplayAbilitySpec(UGA_HitProtection::StaticClass(), 1, INDEX_NONE));
}

const USlimeRunConfig* ASlimeWarCharacter::GetRunConfig() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UStatTableProvider* Provider = GameInstance ? GameInstance->GetSubsystem<UStatTableProvider>() : nullptr;

	return Provider ? Provider->GetRunConfig() : nullptr;
}

void ASlimeWarCharacter::ApplyRunConfig()
{
	const USlimeRunConfig* RunConfig = GetRunConfig();

	if (!RunConfig)
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("ASlimeWarCharacter: no run config, keeping default movement speed and zeroed attributes.")
			TEXT(" Create /Game/_SlimeWar/Core/Data/DA_RunConfig and point USlimeGameSettings at it."));
		return;
	}

	if (RunConfig->PlayerMoveSpeed > 0.f)
	{
		GetCharacterMovement()->MaxWalkSpeed = RunConfig->PlayerMoveSpeed;
	}

	if (RunConfig->PlayerTurnRateDegPerSec > 0.f)
	{
		GetCharacterMovement()->RotationRate = FRotator(0.f, RunConfig->PlayerTurnRateDegPerSec, 0.f);
	}

	if (RunConfig->PlayerAcceleration > 0.f)
	{
		GetCharacterMovement()->MaxAcceleration = RunConfig->PlayerAcceleration;
	}

	if (RunConfig->PlayerBrakingDeceleration > 0.f)
	{
		GetCharacterMovement()->BrakingDecelerationWalking = RunConfig->PlayerBrakingDeceleration;
	}

	// TPS shoulder view (PA-02). Speed / sensitivity stay in Config/DefaultInput.ini.
	if (CameraBoom)
	{
		if (RunConfig->CameraBoomLength > 0.f)
		{
			CameraBoom->TargetArmLength = RunConfig->CameraBoomLength;
		}

		if (!RunConfig->CameraSocketOffset.IsNearlyZero())
		{
			CameraBoom->SocketOffset = RunConfig->CameraSocketOffset;
		}
	}

	if (FollowCamera && RunConfig->CameraFieldOfView > 0.f)
	{
		FollowCamera->SetFieldOfView(RunConfig->CameraFieldOfView);
	}

	if (Attributes)
	{
		Attributes->InitMaxHealth(RunConfig->PlayerMaxHealth);
		Attributes->InitHealth(RunConfig->PlayerMaxHealth);
	}

	FWeaponStatRow WeaponRow;
	UStatTableProvider* Provider = GetGameInstance() ? GetGameInstance()->GetSubsystem<UStatTableProvider>() : nullptr;
	if (Provider && Provider->GetWeaponStat(RunConfig->DefaultWeaponId, WeaponRow))
	{
		if (Attributes)
		{
			Attributes->InitWeaponDamage(WeaponRow.Damage);
			Attributes->InitMagazineSize(static_cast<float>(WeaponRow.MagazineSize));
			Attributes->InitReloadDuration(WeaponRow.ReloadDuration);
			Attributes->InitFireRate(WeaponRow.FireRate);
		}

		if (Weapon)
		{
			Weapon->SetupFromStatRow(RunConfig->DefaultWeaponId, WeaponRow);
		}
	}

	SyncHealthMirror();
}

void ASlimeWarCharacter::SyncHealthMirror()
{
	if (!Health || !Attributes)
	{
		return;
	}

	const float NewHealth = Attributes->GetHealth();

	// Losing health means a damage effect landed (design 3.4): open the protection window.
	if (bHasMirroredHealth && NewHealth < LastMirroredHealth && NewHealth > 0.f)
	{
		if (AbilitySystem)
		{
			AbilitySystem->TryActivateAbilityByClass(UGA_HitProtection::StaticClass());
		}
	}

	LastMirroredHealth = NewHealth;
	bHasMirroredHealth = true;

	Health->SetHealthFromProxy(NewHealth, Attributes->GetMaxHealth());
}

void ASlimeWarCharacter::NotifyDamagedFrom(AActor* Causer)
{
	const FVector Direction = Causer
		? (Causer->GetActorLocation() - GetActorLocation()).GetSafeNormal()
		: FVector::ZeroVector;

	OnPlayerDamaged.Broadcast(Direction);
}

void ASlimeWarCharacter::HandleMirrorDeath()
{
	UE_LOG(LogSlimeWar, Log, TEXT("ASlimeWarCharacter: player died."));

	if (AbilitySystem)
	{
		AbilitySystem->RemoveLooseGameplayTag(TAG_State_Player_Controllable);
		AbilitySystem->AddLooseGameplayTag(TAG_State_Player_Dead);
		AbilitySystem->CancelAbilities();
	}

	bWantsToFire = false;
	bIsAiming = false;

	// Death locks movement (design 3.1). Phase B adds GA_Die on top of this fallback.
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	if (ASlimeWarGameMode* GameMode = GetSlimeGameMode(this))
	{
		GameMode->OnPlayerDied();
	}
}

void ASlimeWarCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Hold-to-fire: the cooldown GameplayEffect is what actually gates the rate, so simply
	// asking again every frame is enough (a blocked activation is silently ignored).
	if (bWantsToFire && AbilitySystem && Health && !Health->IsDead())
	{
		AbilitySystem->TryActivateAbilityByClass(UGA_Fire::StaticClass());
	}

	// Slime.Debug.DrawAimAssist: show what aim assist is doing. Nothing is drawn when it is off.
	if (bIsAiming && SlimeCVars::DebugDrawAimAssist != 0 && FollowCamera && Weapon)
	{
		const FVector CameraLocation = FollowCamera->GetComponentLocation();
		const FVector CameraForward = FollowCamera->GetForwardVector();
		const float Range = Weapon->GetRange();

		AActor* AssistTarget = nullptr;
		const FVector AssistedDirection =
			GetAimDirectionWithAssist(CameraLocation, CameraForward, Range, &AssistTarget);

		// White: where the camera points. Yellow: where the shot will actually go.
		DrawDebugLine(GetWorld(), CameraLocation, CameraLocation + CameraForward * Range,
			FColor::White, false, -1.f, 0, 1.f);
		DrawDebugLine(GetWorld(), CameraLocation, CameraLocation + AssistedDirection * Range,
			FColor::Yellow, false, -1.f, 0, 2.f);

		if (AssistTarget)
		{
			DrawDebugLine(GetWorld(), CameraLocation, AssistTarget->GetActorLocation(),
				FColor::Green, false, -1.f, 0, 3.f);
		}
	}
}

void ASlimeWarCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void ASlimeWarCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASlimeWarCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ASlimeWarCharacter::Look);

		BindAbilityActions(PlayerInputComponent);
	}
	else
	{
		UE_LOG(LogSlimeWar, Error,
			TEXT("'%s' failed to find an Enhanced Input component. This project is built on the Enhanced Input system."),
			*GetNameSafe(this));
	}
}

void ASlimeWarCharacter::BindAbilityActions(UInputComponent* PlayerInputComponent)
{
	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInputComponent)
	{
		return;
	}

	// Enhanced Input asserts on a null action, hence the guards. The assets are assigned in
	// BP_ThirdPersonCharacter (see the Phase A editor checklist).
	if (FireAction)
	{
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Started, this, &ASlimeWarCharacter::OnFirePressed);
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Completed, this, &ASlimeWarCharacter::OnFireReleased);
	}

	if (ReloadAction)
	{
		EnhancedInputComponent->BindAction(ReloadAction, ETriggerEvent::Started, this, &ASlimeWarCharacter::OnReloadPressed);
	}

	if (AimAction)
	{
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Started, this, &ASlimeWarCharacter::OnAimPressed);
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Completed, this, &ASlimeWarCharacter::OnAimReleased);
	}

	if (PauseAction)
	{
		EnhancedInputComponent->BindAction(PauseAction, ETriggerEvent::Started, this, &ASlimeWarCharacter::OnPausePressed);
	}
}

void ASlimeWarCharacter::OnFirePressed()
{
	bWantsToFire = true;

	if (AbilitySystem)
	{
		AbilitySystem->AbilityLocalInputPressed(static_cast<int32>(EAbilityInputID::Fire));
	}
}

void ASlimeWarCharacter::OnFireReleased()
{
	bWantsToFire = false;

	if (AbilitySystem)
	{
		AbilitySystem->AbilityLocalInputReleased(static_cast<int32>(EAbilityInputID::Fire));
	}
}

void ASlimeWarCharacter::OnReloadPressed()
{
	if (AbilitySystem)
	{
		AbilitySystem->AbilityLocalInputPressed(static_cast<int32>(EAbilityInputID::Reload));
	}
}

void ASlimeWarCharacter::OnAimPressed()
{
	bIsAiming = true;

	// While aiming the character faces the camera (strafe shooting) instead of its movement.
	bUseControllerRotationYaw = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;

	if (AbilitySystem)
	{
		AbilitySystem->AbilityLocalInputPressed(static_cast<int32>(EAbilityInputID::Aim));
	}
}

void ASlimeWarCharacter::OnAimReleased()
{
	bIsAiming = false;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

	if (AbilitySystem)
	{
		AbilitySystem->AbilityLocalInputReleased(static_cast<int32>(EAbilityInputID::Aim));
	}
}

void ASlimeWarCharacter::OnPausePressed()
{
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		PlayerController->SetPause(!PlayerController->IsPaused());
	}
}

void ASlimeWarCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ASlimeWarCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

bool ASlimeWarCharacter::HasClearShot(const FVector& From, const AActor* Target) const
{
	UWorld* World = GetWorld();
	if (!World || !Target)
	{
		return false;
	}

	FCollisionQueryParams Params(FName(TEXT("SlimeAimAssistLOS")), false, this);
	Params.AddIgnoredActor(Target);

	FHitResult Hit;
	const bool bBlocked = World->LineTraceSingleByChannel(
		Hit, From, Target->GetActorLocation(), ECC_Visibility, Params);

	return !bBlocked;
}

FVector ASlimeWarCharacter::ComputeAimDirection(AActor*& OutAssistTarget) const
{
	OutAssistTarget = nullptr;

	if (!FollowCamera || !Weapon)
	{
		return FVector::ForwardVector;
	}

	return GetAimDirectionWithAssist(
		FollowCamera->GetComponentLocation(),
		FollowCamera->GetForwardVector(),
		Weapon->GetRange(),
		&OutAssistTarget);
}

FVector ASlimeWarCharacter::GetAimDirectionWithAssist(const FVector& CameraLocation, const FVector& CameraForward, float MaxRange, AActor** OutAssistTarget) const
{
	if (OutAssistTarget)
	{
		*OutAssistTarget = nullptr;
	}

	if (!bIsAiming)
	{
		return CameraForward;
	}

	const USlimeRunConfig* RunConfig = GetRunConfig();
	const USlimeWeaponComponent* WeaponComponent = Weapon;
	const float Strength = WeaponComponent ? WeaponComponent->GetStatRow().AimAssistStrength : 0.f;

	if (!RunConfig || RunConfig->AimAssistMaxAngleDeg <= 0.f || Strength <= 0.f || MaxRange <= 0.f)
	{
		return CameraForward;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return CameraForward;
	}

	const float ConeHalfAngle = FMath::DegreesToRadians(RunConfig->AimAssistMaxAngleDeg);

	// Query around the camera, not at a fixed point down the ray: sampling at "half the weapon
	// range" missed everything closer than that (the whole sandbox), which silently disabled
	// aim assist. The cone test and the line of sight check below do the actual filtering.
	const FVector SampleCenter = CameraLocation;
	const float SampleRadius = MaxRange;

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams Params(FName(TEXT("SlimeAimAssist")), false, this);

	TArray<FOverlapResult> Overlaps;
	if (!World->OverlapMultiByObjectType(
		Overlaps, SampleCenter, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(SampleRadius), Params))
	{
		return CameraForward;
	}

	const AActor* BestTarget = nullptr;
	float BestAngle = ConeHalfAngle;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		const AActor* Candidate = Overlap.GetActor();
		if (!Candidate || Candidate == this)
		{
			continue;
		}

		// IsAuthoritative is true only for enemies (the player health is a proxy mirror),
		// which keeps the Player module free of any Enemy dependency (plan rule 3).
		const USlimeHealthComponent* CandidateHealth = Candidate->FindComponentByClass<USlimeHealthComponent>();
		if (!CandidateHealth || CandidateHealth->IsDead() || !CandidateHealth->IsAuthoritative())
		{
			continue;
		}

		const FVector ToCandidate = (Candidate->GetActorLocation() - CameraLocation).GetSafeNormal();
		const float Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(CameraForward, ToCandidate), -1.f, 1.f));

		if (Angle > ConeHalfAngle || Angle >= BestAngle)
		{
			continue;
		}

		if (!HasClearShot(CameraLocation, Candidate))
		{
			continue;
		}

		BestAngle = Angle;
		BestTarget = Candidate;
	}

	if (!BestTarget)
	{
		return CameraForward;
	}

	if (OutAssistTarget)
	{
		*OutAssistTarget = const_cast<AActor*>(BestTarget);
	}

	const FVector ToTarget = (BestTarget->GetActorLocation() - CameraLocation).GetSafeNormal();
	return FMath::Lerp(CameraForward, ToTarget, FMath::Clamp(Strength, 0.f, 1.f)).GetSafeNormal();
}

bool ASlimeWarCharacter::PerformShot(FVector& OutImpactPoint, AActor*& OutHitActor)
{
	OutImpactPoint = FVector::ZeroVector;
	OutHitActor = nullptr;

	USlimeWeaponComponent* WeaponComponent = Weapon;
	if (!WeaponComponent || !FollowCamera)
	{
		return false;
	}

	const float Range = WeaponComponent->GetRange();
	if (Range <= 0.f)
	{
		UE_LOG(LogSlimeWar, Error, TEXT("ASlimeWarCharacter::PerformShot: weapon range is not configured."));
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector CameraLocation = FollowCamera->GetComponentLocation();
	const FVector CameraForward = FollowCamera->GetForwardVector();
	const FVector ShotDirection = GetAimDirectionWithAssist(CameraLocation, CameraForward, Range);

	FCollisionQueryParams Params(FName(TEXT("SlimeShot")), false, this);
	Params.AddIgnoredActor(this);

	// 1) Camera trace: where is the player actually aiming?
	FHitResult CameraHit;
	const FVector CameraEnd = CameraLocation + ShotDirection * Range;
	const bool bCameraHit = World->LineTraceSingleByChannel(
		CameraHit, CameraLocation, CameraEnd, ECC_Visibility, Params);

	const FVector AimPoint = bCameraHit ? CameraHit.ImpactPoint : CameraEnd;
	AActor* AimActor = bCameraHit ? CameraHit.GetActor() : nullptr;

	// 2) Muzzle trace: the barrel may be inside geometry even when the camera is not.
	const USlimeRunConfig* RunConfig = GetRunConfig();
	const FVector MuzzleOffset = RunConfig ? RunConfig->MuzzleOffsetLocal : FVector::ZeroVector;
	const FVector MuzzleLocation = CameraLocation + FollowCamera->GetComponentRotation().RotateVector(MuzzleOffset);

	FHitResult MuzzleHit;
	const bool bMuzzleBlocked = World->LineTraceSingleByChannel(
		MuzzleHit, MuzzleLocation, AimPoint, ECC_Visibility, Params);

	if (bMuzzleBlocked && MuzzleHit.GetActor() != AimActor)
	{
		// Muzzle is buried in geometry: the shot hits the wall instead (design 3.3).
		OutImpactPoint = MuzzleHit.ImpactPoint;
		WeaponComponent->BroadcastShotResult(nullptr, OutImpactPoint, false);
		return true;
	}

	OutImpactPoint = AimPoint;
	OutHitActor = AimActor;

	if (AimActor)
	{
		// Single damage entry point: ASC targets get GE_Damage, enemies get their HealthComponent.
		if (USlimeCombatSubsystem* Combat = USlimeCombatSubsystem::Get(this))
		{
			Combat->ApplyDamageTo(AimActor, WeaponComponent->GetDamage(), this);
		}
	}

	WeaponComponent->BroadcastShotResult(AimActor, AimPoint, AimActor != nullptr);
	return true;
}
