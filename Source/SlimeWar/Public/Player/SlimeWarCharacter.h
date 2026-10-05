// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "SlimeWarCharacter.generated.h"

class UAbilitySystemComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class USlimeHealthComponent;
class USlimePlayerAttributeSet;
class USlimeRunConfig;
class USlimeWeaponComponent;
class USpringArmComponent;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSlimePlayerDamagedSignature, FVector, DamageDirection);

/**
 * Player character. GAS is fully enabled on this actor only (plan decision: layered GAS).
 *
 * Health ownership: the AttributeSet is authoritative, USlimeHealthComponent runs in proxy
 * mode and mirrors it. OnDeath is therefore the one and only death signal the rest of the
 * project ever sees.
 *
 * Phase A: TPS shoulder camera, aim assist, hold-to-fire, magazine + reload, hit protection.
 */
UCLASS(config = Game)
class ASlimeWarCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ASlimeWarCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void Tick(float DeltaSeconds) override;

	/** Returns CameraBoom subobject. */
	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject. */
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	/** Proxy health component: mirrored from the AttributeSet, broadcasts the single OnDeath. */
	FORCEINLINE USlimeHealthComponent* GetHealthComponent() const { return Health; }

	FORCEINLINE USlimeWeaponComponent* GetWeaponComponent() const { return Weapon; }

	UFUNCTION(BlueprintPure, Category = "Slime|Player")
	bool IsAiming() const { return bIsAiming; }

	/**
	 * Camera-space fire direction after aim assist. OutAssistTarget is set to the actor
	 * aim assist pulled toward, or null when it did nothing. Used by PerformShot and by the
	 * Slime.Debug.DrawAimAssist visualisation.
	 */
	FVector ComputeAimDirection(AActor*& OutAssistTarget) const;

	/**
	 * One shot: aim assist, camera trace, muzzle occlusion check, then damage through
	 * USlimeCombatSubsystem. Always consumes the round; reports miss through OutHitActor == null.
	 */
	bool PerformShot(FVector& OutImpactPoint, AActor*& OutHitActor);

	/** Called by USlimePlayerAttributeSet when a damage effect lands. Sets the direction hook. */
	void NotifyDamagedFrom(AActor* Causer);

	/** Directional damage hint for Phase D UI. Does not move the aim centre (design 8.2). */
	UPROPERTY(BlueprintAssignable, Category = "Slime|Player")
	FSlimePlayerDamagedSignature OnPlayerDamaged;

protected:
	virtual void BeginPlay() override;
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// -- Movement / camera input --
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

	/** Routes the ability input actions to the ASC (Phase A: Fire / Reload / Aim / Pause). */
	void BindAbilityActions(UInputComponent* PlayerInputComponent);

	UFUNCTION() void OnFirePressed();
	UFUNCTION() void OnFireReleased();
	UFUNCTION() void OnReloadPressed();
	UFUNCTION() void OnAimPressed();
	UFUNCTION() void OnAimReleased();
	UFUNCTION() void OnPausePressed();

	// -- GAS --
	void InitAbilitySystem();
	void ApplyRunConfig();
	void SyncHealthMirror();

	UFUNCTION() void HandleMirrorDeath();

	/** Aim assist cone search; returns CameraForward unchanged when not aiming or nothing is close. */
	FVector GetAimDirectionWithAssist(const FVector& CameraLocation, const FVector& CameraForward, float MaxRange, AActor** OutAssistTarget = nullptr) const;

	bool HasClearShot(const FVector& From, const AActor* Target) const;

	const USlimeRunConfig* GetRunConfig() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY()
	TObjectPtr<USlimePlayerAttributeSet> Attributes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|Health")
	TObjectPtr<USlimeHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|Weapon")
	TObjectPtr<USlimeWeaponComponent> Weapon;

	// -- Camera --
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	// -- Input (mapping context + move/look; ability actions are filled in the Blueprint) --
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Ability", meta = (AllowPrivateAccess = "true"))
	UInputAction* FireAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Ability", meta = (AllowPrivateAccess = "true"))
	UInputAction* ReloadAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Ability", meta = (AllowPrivateAccess = "true"))
	UInputAction* AimAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Ability", meta = (AllowPrivateAccess = "true"))
	UInputAction* PauseAction;

	// -- Phase A runtime state --

	/** True between Fire pressed and released; re-triggers the ability while the cooldown allows. */
	bool bWantsToFire = false;

	/** Right mouse held: enables aim assist and strafe facing. */
	bool bIsAiming = false;

	/** Previous mirrored health, used to detect "just took damage" for hit protection. */
	float LastMirroredHealth = 0.f;
	bool bHasMirroredHealth = false;
};
