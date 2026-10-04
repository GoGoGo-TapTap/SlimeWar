// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/SlimeWarCharacter.h"

#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/SlimeHealthComponent.h"
#include "Core/SlimeWarLog.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameplayFramework/SlimeRunConfig.h"
#include "GameplayFramework/SlimeWarGameMode.h"
#include "GameplayFramework/StatTableProvider.h"
#include "InputActionValue.h"
#include "Player/SlimeAbilityInputID.h"
#include "Player/SlimePlayerAttributeSet.h"
#include "Player/SlimeWeaponComponent.h"

ASlimeWarCharacter::ASlimeWarCharacter()
{
	// Capsule / camera keep the template defaults; Phase A moves the tuning values into data.
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
}

void ASlimeWarCharacter::ApplyRunConfig()
{
	UStatTableProvider* Provider = GetGameInstance() ? GetGameInstance()->GetSubsystem<UStatTableProvider>() : nullptr;
	const USlimeRunConfig* RunConfig = Provider ? Provider->GetRunConfig() : nullptr;

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

	if (Attributes)
	{
		Attributes->InitMaxHealth(RunConfig->PlayerMaxHealth);
		Attributes->InitHealth(RunConfig->PlayerMaxHealth);
	}

	FWeaponStatRow WeaponRow;
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

	Health->SetHealthFromProxy(Attributes->GetHealth(), Attributes->GetMaxHealth());
}

void ASlimeWarCharacter::HandleMirrorDeath()
{
	UE_LOG(LogSlimeWar, Log, TEXT("ASlimeWarCharacter: player died."));

	if (ASlimeWarGameMode* GameMode = GetSlimeGameMode(this))
	{
		GameMode->OnPlayerDied();
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

	// Phase 0 stub: no IA assets exist yet, so bind only the actions that are actually assigned.
	// Enhanced Input asserts on a null action, hence the guards.
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
	if (AbilitySystem)
	{
		AbilitySystem->AbilityLocalInputPressed(static_cast<int32>(EAbilityInputID::Fire));
	}
}

void ASlimeWarCharacter::OnFireReleased()
{
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
	if (AbilitySystem)
	{
		AbilitySystem->AbilityLocalInputPressed(static_cast<int32>(EAbilityInputID::Aim));
	}
}

void ASlimeWarCharacter::OnAimReleased()
{
	if (AbilitySystem)
	{
		AbilitySystem->AbilityLocalInputReleased(static_cast<int32>(EAbilityInputID::Aim));
	}
}

void ASlimeWarCharacter::OnPausePressed()
{
	if (AbilitySystem)
	{
		AbilitySystem->AbilityLocalInputPressed(static_cast<int32>(EAbilityInputID::Pause));
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
