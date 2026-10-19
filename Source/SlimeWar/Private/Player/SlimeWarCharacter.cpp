// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/SlimeWarCharacter.h"

#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Core/SlimeGameplayTags.h"
#include "Core/SlimeHealthComponent.h"
#include "Core/SlimeWarCollisionChannels.h"
#include "Core/SlimeWarCVars.h"
#include "Core/SlimeWarLog.h"
#include "DrawDebugHelpers.h"
#include "Ecology/SlimeBodyInteractionComponent.h"
#include "Ecology/SlimeEcologyEnemy.h"
#include "Ecology/SlimeEcologyFusionComponent.h"
#include "Ecology/SlimeEcologyRules.h"
#include "Ecology/SlimeEcologySubsystem.h"
#include "Ecology/SlimeEcologyGrenade.h"
#include "Ecology/SlimeWeakpointComponent.h"
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
#include "Player/Abilities/GA_Die.h"
#include "Player/Abilities/GA_ChargeFire.h"
#include "Player/Abilities/GA_Fire.h"
#include "Player/Abilities/GA_HitProtection.h"
#include "Player/Abilities/GA_Reload.h"
#include "Player/SlimeAbilityInputID.h"
#include "Player/SlimeAimAssistComponent.h"
#include "Player/SlimePlayerAttributeSet.h"
#include "Player/SlimeWeaponComponent.h"
#include "Player/SlimeWeaponRules.h"

ASlimeWarCharacter::ASlimeWarCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(43.f, 90.0f);

	// Normal slimes sit on their own object channel and the player *overlaps* them instead of
	// blocking. Blocking two capsules makes the player's sweeps push the slime out of the way
	// ("kicked across the arena"), which can also break a fusion that is being set up; nothing in
	// the design knocks slimes back. Aggro slimes keep blocking each other and normal slimes
	// (PB-16) because they stay on ECC_Pawn and keep the default Block response.
	GetCapsuleComponent()->SetCollisionResponseToChannel(SlimeCollisionChannels::Fusion, ECR_Overlap);

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
	AimAssist = CreateDefaultSubobject<USlimeAimAssistComponent>(TEXT("AimAssist"));
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

	// Controllable by default: Phase D's URunSubsystem pushes the real state within a frame of
	// world begin play (Idle / Deploying / Result all lock the player, Running unlocks it), and
	// keeping the default unlocked means a map without a run subsystem still plays.
	AbilitySystem->AddLooseGameplayTag(TAG_State_Player_Controllable);

	// Input ids line up with EAbilityInputID so AbilityLocalInputPressed reaches them.
	AbilitySystem->GiveAbility(FGameplayAbilitySpec(
		UGA_ChargeFire::StaticClass(), 1, INDEX_NONE));
	AbilitySystem->GiveAbility(FGameplayAbilitySpec(
		UGA_Reload::StaticClass(), 1, static_cast<int32>(EAbilityInputID::Reload)));

	// Hit protection is triggered by code, not by an input.
	AbilitySystem->GiveAbility(FGameplayAbilitySpec(UGA_HitProtection::StaticClass(), 1, INDEX_NONE));

	// PA-11: death runs as an ability so it can cancel the others before the Dead tag goes up.
	AbilitySystem->GiveAbility(FGameplayAbilitySpec(UGA_Die::StaticClass(), 1, INDEX_NONE));
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

	if (const USlimeEcologyRules* EcologyRules = USlimeEcologyRules::GetFor(this))
	{
		AssimilationGrenades = EcologyRules->GrenadesPerType;
		TornadoGrenades = EcologyRules->GrenadesPerType;
		OnGrenadeInventoryChanged.Broadcast(AssimilationGrenades, TornadoGrenades);
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

	UStatTableProvider* Provider = GetGameInstance() ? GetGameInstance()->GetSubsystem<UStatTableProvider>() : nullptr;
	FWeaponStatRow WeaponRow;
	bool bHasWeaponRow = false;
	if (Provider && Provider->GetWeaponRules())
	{
		const USlimeWeaponRules* WeaponRules = Provider->GetWeaponRules();
		WeaponRow.Damage = WeaponRules->BaseDamage;
		WeaponRow.FireRate = 1.f / FMath::Max(0.01f, WeaponRules->FireInterval);
		WeaponRow.MagazineSize = WeaponRules->MagazineSize;
		WeaponRow.ReloadDuration = WeaponRules->ReloadDuration;
		WeaponRow.Range = WeaponRules->Range;
		WeaponRow.AimAssistStrength = 1.f;
		bHasWeaponRow = true;
	}
	else if (Provider)
	{
		bHasWeaponRow = Provider->GetWeaponStat(RunConfig->DefaultWeaponId, WeaponRow);
	}

	if (bHasWeaponRow)
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
	if (bDeathHandled)
	{
		return;
	}
	bDeathHandled = true;

	UE_LOG(LogSlimeWar, Log, TEXT("ASlimeWarCharacter: player died."));

	// PA-11: the death ability owns "cancel the rest -> tag Dead -> report". If it is missing
	// (misconfigured ASC) fall back to the inline Phase A path so death still works.
	const bool bAbilityStarted = AbilitySystem
		&& AbilitySystem->TryActivateAbilityByClass(UGA_Die::StaticClass());

	if (!bAbilityStarted)
	{
		ApplyDeathEffects(/*bCancelAbilities=*/true);
	}
}

void ASlimeWarCharacter::ApplyDeathEffects(bool bCancelAbilities)
{
	if (AbilitySystem)
	{
		if (bCancelAbilities)
		{
			AbilitySystem->CancelAllAbilities();
		}

		// Loose tags: the dead state must outlive the ability that set it.
		AbilitySystem->RemoveLooseGameplayTag(TAG_State_Player_Controllable);
		AbilitySystem->AddLooseGameplayTag(TAG_State_Player_Dead);
	}

	bCharging = false;
	PendingChargeAlpha = 0.f;
	EndReloadVisuals();
	if (AimAssist)
	{
		AimAssist->ReleaseLock();
	}
	bIsAiming = false;
	bInputBlocked = true;

	// Death locks movement (design 3.1).
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	// The only death exit point the rest of the project sees (plan rule 6).
	if (ASlimeWarGameMode* GameMode = GetSlimeGameMode(this))
	{
		GameMode->OnPlayerDied();
	}
}

void ASlimeWarCharacter::SetRunInputBlocked(bool bBlocked)
{
	// Death wins: the result push must never hand control back to a dead player.
	if (!bBlocked && Health && Health->IsDead())
	{
		return;
	}

	if (bInputBlocked == bBlocked)
	{
		return;
	}

	bInputBlocked = bBlocked;

	if (AbilitySystem)
	{
		if (bBlocked)
		{
			AbilitySystem->RemoveLooseGameplayTag(TAG_State_Player_Controllable);
			AbilitySystem->AddLooseGameplayTag(TAG_State_Player_Deploying);
		}
		else
		{
			AbilitySystem->RemoveLooseGameplayTag(TAG_State_Player_Deploying);
			AbilitySystem->RemoveLooseGameplayTag(TAG_State_Player_Result);
			AbilitySystem->AddLooseGameplayTag(TAG_State_Player_Controllable);
		}
	}

	if (bBlocked)
	{
		// Drop any input state that is still held down, so the lock cannot leave a latched shot
		// or a strafe-facing character behind.
		bCharging = false;
		PendingChargeAlpha = 0.f;
		EndReloadVisuals();
		if (AimAssist)
		{
			AimAssist->ReleaseLock();
		}
		bIsAiming = false;
		bUseControllerRotationYaw = false;
		GetCharacterMovement()->bOrientRotationToMovement = true;

		if (AbilitySystem)
		{
			AbilitySystem->AbilityLocalInputReleased(static_cast<int32>(EAbilityInputID::Fire));
			AbilitySystem->AbilityLocalInputReleased(static_cast<int32>(EAbilityInputID::Aim));
		}

		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}
	}
}

void ASlimeWarCharacter::EnterResultState()
{
	bInputBlocked = true;
	bCharging = false;
	PendingChargeAlpha = 0.f;
	EndReloadVisuals();
	if (AimAssist)
	{
		AimAssist->ReleaseLock();
	}
	bIsAiming = false;

	if (AbilitySystem)
	{
		// PA-10 run-end half: a reload that is in flight must not survive the clock running out.
		AbilitySystem->CancelAllAbilities();

		AbilitySystem->RemoveLooseGameplayTag(TAG_State_Player_Controllable);
		AbilitySystem->RemoveLooseGameplayTag(TAG_State_Player_Deploying);
		AbilitySystem->AddLooseGameplayTag(TAG_State_Player_Result);
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}
}

void ASlimeWarCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ApplyBodyPushToNearbyEcology();
	GrenadeCooldownRemaining = FMath::Max(0.f, GrenadeCooldownRemaining - DeltaSeconds);

	// Charge is accumulated only while the fire button is held; the ability fires on release.
	if (bReloadVisualActive && GetWorld()
		&& GetWorld()->GetTimeSeconds() - ReloadVisualStart >= ReloadVisualDuration)
	{
		EndReloadVisuals();
	}
}

float ASlimeWarCharacter::GetChargeAlpha() const
{
	const USlimeWeaponRules* Rules = USlimeWeaponRules::GetFor(this);
	const UWorld* World = GetWorld();
	if (!bCharging || !Rules || !World)
	{
		return 0.f;
	}

	const float HoldTime = World->GetTimeSeconds() - ChargeStartTime;
	const float ChargeWindow =
		FMath::Max(0.01f, Rules->ChargeFullTime - Rules->ChargeStart);
	return FMath::Clamp((HoldTime - Rules->ChargeStart) / ChargeWindow, 0.f, 1.f);
}

float ASlimeWarCharacter::ConsumeChargeAlpha()
{
	const float Result = PendingChargeAlpha;
	PendingChargeAlpha = 0.f;
	return Result;
}

void ASlimeWarCharacter::BeginReloadVisuals(float Duration)
{
	bCharging = false;
	PendingChargeAlpha = 0.f;
	bReloadVisualActive = Duration > 0.f;
	ReloadVisualDuration = FMath::Max(0.f, Duration);
	ReloadVisualStart = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

void ASlimeWarCharacter::EndReloadVisuals()
{
	bReloadVisualActive = false;
	ReloadVisualDuration = 0.f;
}

float ASlimeWarCharacter::GetReloadProgress() const
{
	if (!bReloadVisualActive || !GetWorld() || ReloadVisualDuration <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}
	return FMath::Clamp(
		(GetWorld()->GetTimeSeconds() - ReloadVisualStart) / ReloadVisualDuration,
		0.f,
		1.f);
}

bool ASlimeWarCharacter::CanStartCharge() const
{
	const USlimeWeaponComponent* WeaponComponent = Weapon;
	if (bInputBlocked || !Health || Health->IsDead() || !WeaponComponent
		|| WeaponComponent->IsMagazineEmpty() || bReloadVisualActive)
	{
		return false;
	}

	return !AbilitySystem
		|| !AbilitySystem->HasMatchingGameplayTag(TAG_State_Weapon_Reloading);
}

void ASlimeWarCharacter::ApplyBodyPushToNearbyEcology()
{
	if (bInputBlocked || !Health || Health->IsDead())
	{
		return;
	}

	const UWorld* World = GetWorld();
	const USlimeEcologySubsystem* Ecology = World
		? World->GetSubsystem<USlimeEcologySubsystem>()
		: nullptr;
	const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this);
	const UCapsuleComponent* PlayerCapsule = GetCapsuleComponent();
	if (!Ecology || !Rules || !PlayerCapsule)
	{
		return;
	}

	const FVector PlayerLocation = GetActorLocation();
	const FVector PlayerVelocity = GetVelocity();
	const float PlayerSpeed = PlayerVelocity.Size2D();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	const FVector ControlInput = GetLastMovementInputVector();
	const bool bHasControlInput = ControlInput.SizeSquared2D() > KINDA_SMALL_NUMBER;
	const bool bHasRecentMoveInput =
		Now - LastMoveInputTime <= 0.15f && !LastMoveDirection.IsNearlyZero();
	if (PlayerSpeed < 10.f && !bHasRecentMoveInput && !bHasControlInput)
	{
		return;
	}

	TArray<ASlimeEcologyEnemy*> Neighbours;
	Ecology->QueryAgentsAroundLocation(
		PlayerLocation,
		PlayerCapsule->GetScaledCapsuleRadius()
			+ Rules->BodyPushContactPadding
			+ 80.f,
		Neighbours);

	const FVector MoveDirection = PlayerSpeed >= 10.f
		? PlayerVelocity.GetSafeNormal2D()
		: (!LastMoveDirection.IsNearlyZero()
			? LastMoveDirection
			: ControlInput.GetSafeNormal2D());
	const float MaxPlayerSpeed = FMath::Max(1.f, GetCharacterMovement()->MaxWalkSpeed);
	const float EffectivePlayerSpeed = PlayerSpeed >= 10.f
		? PlayerSpeed
		: MaxPlayerSpeed * 0.5f;
	for (ASlimeEcologyEnemy* Slime : Neighbours)
	{
		const UCapsuleComponent* SlimeCapsule = Slime ? Slime->GetCapsuleComponent() : nullptr;
		USlimeBodyInteractionComponent* Body = Slime ? Slime->GetBodyInteraction() : nullptr;
		if (!Slime || !SlimeCapsule || !Body || Slime->GetMass() > Rules->BodyPushMaxMass)
		{
			continue;
		}

		const FVector ToSlime = Slime->GetActorLocation() - PlayerLocation;
		const float ContactDistance =
			PlayerCapsule->GetScaledCapsuleRadius()
			+ SlimeCapsule->GetScaledCapsuleRadius()
			+ Rules->BodyPushContactPadding;
		if (ToSlime.Size2D() > ContactDistance
			|| FMath::Abs(ToSlime.Z) > 120.f)
		{
			continue;
		}

		FVector PushDirection =
			(ToSlime.GetSafeNormal2D() * 0.65f + MoveDirection * 0.35f).GetSafeNormal2D();
		if (PushDirection.IsNearlyZero())
		{
			PushDirection = MoveDirection;
		}

		const float SpeedScale =
			1.f + (EffectivePlayerSpeed / MaxPlayerSpeed) * Rules->BodyPushPlayerSpeedScale;
		const float MassScale =
			FMath::Pow(static_cast<float>(FMath::Max(1, Slime->GetMass())), Rules->BodyPushMassExponent);
		Body->ReceiveBodyPush(
			PushDirection,
			Rules->BodyPushBaseSpeed * SpeedScale / MassScale,
			this);
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

	PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &ASlimeWarCharacter::SelectAssimilationGrenade);
	PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ASlimeWarCharacter::SelectTornadoGrenade);
	PlayerInputComponent->BindKey(EKeys::Z, IE_Pressed, this, &ASlimeWarCharacter::SelectFactionA);
	PlayerInputComponent->BindKey(EKeys::X, IE_Pressed, this, &ASlimeWarCharacter::SelectFactionB);
	PlayerInputComponent->BindKey(EKeys::G, IE_Pressed, this, &ASlimeWarCharacter::ThrowSelectedGrenade);
}

void ASlimeWarCharacter::OnFirePressed()
{
	if (bInputBlocked || !Health || Health->IsDead())
	{
		return;
	}

	if (!CanStartCharge())
	{
		if (Weapon && Weapon->IsMagazineEmpty() && AbilitySystem
			&& !bReloadVisualActive
			&& !AbilitySystem->HasMatchingGameplayTag(TAG_State_Weapon_Reloading))
		{
			AbilitySystem->TryActivateAbilityByClass(UGA_Reload::StaticClass());
		}
		return;
	}

	bCharging = true;
	ChargeStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	PendingChargeAlpha = 0.f;
}

void ASlimeWarCharacter::OnFireReleased()
{
	if (!bCharging)
	{
		return;
	}

	PendingChargeAlpha = GetChargeAlpha();
	bCharging = false;

	if (AbilitySystem && !bInputBlocked && Health && !Health->IsDead())
	{
		AbilitySystem->TryActivateAbilityByClass(UGA_ChargeFire::StaticClass());
	}
}

void ASlimeWarCharacter::OnReloadPressed()
{
	if (bInputBlocked)
	{
		return;
	}

	bCharging = false;
	PendingChargeAlpha = 0.f;
	if (AbilitySystem)
	{
		AbilitySystem->AbilityLocalInputPressed(static_cast<int32>(EAbilityInputID::Reload));
	}
}

void ASlimeWarCharacter::OnAimPressed()
{
	if (bInputBlocked)
	{
		return;
	}

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
	// Phase D: the UI layer owns pausing (it has to show a menu and set the input mode, and an
	// engine pause without that leaves the buttons unclickable). The character only reports the
	// key, so the Player module never has to know a UI exists.
	OnPauseRequested.Broadcast();
}

void ASlimeWarCharacter::SelectAssimilationGrenade()
{
	SelectedGrenadeType = ESlimeGrenadeType::Assimilation;
	OnGrenadeSelectionChanged.Broadcast(SelectedGrenadeType, SelectedGrenadeFaction);
}

void ASlimeWarCharacter::SelectTornadoGrenade()
{
	SelectedGrenadeType = ESlimeGrenadeType::Tornado;
	OnGrenadeSelectionChanged.Broadcast(SelectedGrenadeType, SelectedGrenadeFaction);
}

void ASlimeWarCharacter::SelectFactionA()
{
	SelectedGrenadeFaction = ESlimeFaction::A;
	OnGrenadeSelectionChanged.Broadcast(SelectedGrenadeType, SelectedGrenadeFaction);
}

void ASlimeWarCharacter::SelectFactionB()
{
	SelectedGrenadeFaction = ESlimeFaction::B;
	OnGrenadeSelectionChanged.Broadcast(SelectedGrenadeType, SelectedGrenadeFaction);
}

void ASlimeWarCharacter::ThrowSelectedGrenade()
{
	if (bInputBlocked || GrenadeCooldownRemaining > 0.f
		|| !Health || Health->IsDead())
	{
		return;
	}

	int32& Stock = SelectedGrenadeType == ESlimeGrenadeType::Assimilation
		? AssimilationGrenades
		: TornadoGrenades;
	if (Stock <= 0 || !FollowCamera)
	{
		return;
	}

	const USlimeEcologyRules* Rules = USlimeEcologyRules::GetFor(this);
	UWorld* World = GetWorld();
	if (!Rules || !World)
	{
		return;
	}

	const FVector SpawnLocation =
		FollowCamera->GetComponentLocation()
		+ FollowCamera->GetForwardVector() * 80.f;
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASlimeEcologyGrenade* Grenade = World->SpawnActor<ASlimeEcologyGrenade>(
		ASlimeEcologyGrenade::StaticClass(),
		SpawnLocation,
		FollowCamera->GetComponentRotation(),
		Params);
	if (!Grenade)
	{
		return;
	}

	Grenade->Launch(
		SelectedGrenadeType,
		SelectedGrenadeFaction,
		FollowCamera->GetForwardVector());
	--Stock;
	GrenadeCooldownRemaining = Rules->GrenadeThrowInterval;
	OnGrenadeInventoryChanged.Broadcast(AssimilationGrenades, TornadoGrenades);
	UE_LOG(LogSlimeWar, Log,
		TEXT("GrenadeThrow: %s faction %d (A=%d T=%d)."),
		ASlimeEcologyGrenade::TypeName(SelectedGrenadeType),
		static_cast<int32>(SelectedGrenadeFaction),
		AssimilationGrenades,
		TornadoGrenades);
}

void ASlimeWarCharacter::Move(const FInputActionValue& Value)
{
	if (bInputBlocked)
	{
		return;
	}

	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		LastMoveDirection =
			(ForwardDirection * MovementVector.Y + RightDirection * MovementVector.X)
			.GetSafeNormal2D();
		LastMoveInputTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ASlimeWarCharacter::Look(const FInputActionValue& Value)
{
	if (bInputBlocked)
	{
		return;
	}

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
	// Normal slimes sit on their own object channel (Config/DefaultEngine.ini "SlimeFusion");
	// without this the assist cone would silently stop seeing them.
	ObjectParams.AddObjectTypesToQuery(SlimeCollisionChannels::Fusion);
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
	bool bWeakpoint = false;
	return PerformChargedShot(0.f, OutImpactPoint, OutHitActor, bWeakpoint);
}

bool ASlimeWarCharacter::PerformChargedShot(
	float ChargeAlpha,
	FVector& OutImpactPoint,
	AActor*& OutHitActor,
	bool& bOutWeakpoint)
{
	OutImpactPoint = FVector::ZeroVector;
	OutHitActor = nullptr;
	bOutWeakpoint = false;

	USlimeWeaponComponent* WeaponComponent = Weapon;
	const USlimeWeaponRules* Rules = USlimeWeaponRules::GetFor(this);
	if (!WeaponComponent || !AimAssist || !Rules)
	{
		return false;
	}

	FSlimeShotSolution Shot;
	if (!AimAssist->SolveShot(Shot))
	{
		return false;
	}

	const float Alpha = FMath::Clamp(ChargeAlpha, 0.f, 1.f);
	const float Damage = Rules->BaseDamage * (1.f - Alpha);
	if (Shot.TargetActor && Damage > 0.f)
	{
		if (USlimeCombatSubsystem* Combat = USlimeCombatSubsystem::Get(this))
		{
			Combat->ApplyDamageToComponent(
				Shot.TargetActor,
				Shot.HitComponent,
				Damage,
				this);
		}
	}

	const USlimeEcologyRules* EcologyRules = USlimeEcologyRules::GetFor(this);
	if (ASlimeEcologyEnemy* DirectTarget = Cast<ASlimeEcologyEnemy>(Shot.TargetActor))
	{
		if (USlimeBodyInteractionComponent* Body = DirectTarget->GetBodyInteraction())
		{
			FVector Push = DirectTarget->GetActorLocation() - GetActorLocation();
			Push.Z = 0.f;
			Push = Push.GetSafeNormal2D();
			const float MassScale = FMath::Pow(
				static_cast<float>(FMath::Max(1, DirectTarget->GetMass())),
				Rules->DirectPushMassExponent);
			Body->ApplyExternalVelocity(Push * Rules->DirectPushSpeed / MassScale, this);
			if (EcologyRules)
			{
				Body->ReceiveStun(EcologyRules->HitStunDuration);
				if (Alpha > 0.f
					&& Rules->ChargePushSpeed * Alpha >= EcologyRules->ImpactSplitSpeed
					&& DirectTarget->GetMass() >= 2)
				{
					Body->OpenImpactSplitWindow(EcologyRules->ImpactSplitWindow);
				}
				if (USlimeEcologyFusionComponent* Fusion = DirectTarget->GetEcologyFusionComponent())
				{
					Fusion->InterruptFusion(EcologyRules->FusionInterruptedBlock);
				}
			}
		}
	}

	if (Alpha > 0.f && EcologyRules)
	{
		const float Radius = Rules->ChargePushRadius * Alpha;
		const UWorld* World = GetWorld();
		const USlimeEcologySubsystem* Ecology =
			World ? World->GetSubsystem<USlimeEcologySubsystem>() : nullptr;
		if (Ecology && Radius > 0.f)
		{
			TArray<ASlimeEcologyEnemy*> Neighbours;
			Ecology->QueryAgentsAroundLocation(Shot.AimPoint, Radius, Neighbours);
			for (ASlimeEcologyEnemy* Other : Neighbours)
			{
				if (!Other || Other->IsActorBeingDestroyed())
				{
					continue;
				}

				FCollisionQueryParams Params(SCENE_QUERY_STAT(SlimeChargePushLOS), false, Other);
				FHitResult Hit;
				if (World->LineTraceSingleByChannel(
					Hit,
					Shot.AimPoint,
					Other->GetActorLocation(),
					ECC_WorldStatic,
					Params))
				{
					continue;
				}

				const float Distance =
					FVector::Dist2D(Shot.AimPoint, Other->GetActorLocation());
				const float Falloff =
					1.f - Rules->RadialPushEdgeScale * FMath::Clamp(Distance / Radius, 0.f, 1.f);
				const float PushSpeed = Rules->ChargePushSpeed * Alpha * Falloff;
				if (PushSpeed <= 0.f)
				{
					continue;
				}

				FVector Push = Other->GetActorLocation() - Shot.AimPoint;
				Push.Z = 0.f;
				Push = Push.GetSafeNormal2D();
				if (Push.IsNearlyZero())
				{
					Push = (Other->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
				}
				if (Push.IsNearlyZero())
				{
					Push = GetActorForwardVector().GetSafeNormal2D();
				}
				if (USlimeBodyInteractionComponent* Body = Other->GetBodyInteraction())
				{
					Body->ApplyExternalVelocity(Push * PushSpeed, this);
					Body->ReceiveStun(EcologyRules->HitStunDuration);
					if (PushSpeed >= EcologyRules->ImpactSplitSpeed
						&& Other->GetMass() >= 2)
					{
						Body->OpenImpactSplitWindow(EcologyRules->ImpactSplitWindow);
					}
				}
				if (USlimeEcologyFusionComponent* Fusion = Other->GetEcologyFusionComponent())
				{
					Fusion->InterruptFusion(EcologyRules->FusionInterruptedBlock);
				}
			}
		}
	}

	OutImpactPoint = Shot.AimPoint;
	OutHitActor = Shot.TargetActor;
	bOutWeakpoint = Shot.bWeakpoint;
	WeaponComponent->BroadcastShotResult(
		Shot.TargetActor,
		Shot.AimPoint,
		Shot.TargetActor != nullptr);
	return true;
}
