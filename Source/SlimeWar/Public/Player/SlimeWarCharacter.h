// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Core/SlimeWarCoreTypes.h"
#include "Ecology/SlimeEcologyGrenade.h"
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
class USlimeAimAssistComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPostProcessComponent;
class USpringArmComponent;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSlimePlayerDamagedSignature, FVector, DamageDirection);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSlimePauseRequestedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FSlimeGrenadeInventoryChangedSignature, int32, Assimilation, int32, Tornado);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FSlimeGrenadeSelectionChangedSignature, ESlimeGrenadeType, Type, ESlimeFaction, Faction);

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

	FORCEINLINE USlimeAimAssistComponent* GetAimAssistComponent() const { return AimAssist; }

	UFUNCTION(BlueprintPure, Category = "Slime|Player")
	bool IsAiming() const { return bIsAiming; }

	UFUNCTION(BlueprintPure, Category = "Slime|Player")
	bool IsSprinting() const { return bIsSprinting && !bIsAiming; }

	UFUNCTION(BlueprintPure, Category = "Slime|Player")
	bool IsUsingLeftShoulder() const { return bIsLeftShoulder; }

	void DebugSetSprinting(bool bEnabled);
	void DebugSetAiming(bool bEnabled);
	void DebugJump();
	void DebugSwitchShoulder();

        UFUNCTION(BlueprintPure, Category = "Slime|Player")
        ESlimeGrenadeType GetSelectedGrenadeType() const { return SelectedGrenadeType; }

        UFUNCTION(BlueprintPure, Category = "Slime|Player")
        ESlimeFaction GetSelectedGrenadeFaction() const { return SelectedGrenadeFaction; }

        UFUNCTION(BlueprintPure, Category = "Slime|Player")
        int32 GetAssimilationGrenadeCount() const { return AssimilationGrenades; }

        UFUNCTION(BlueprintPure, Category = "Slime|Player")
        int32 GetTornadoGrenadeCount() const { return TornadoGrenades; }

	/** Total damage taken this run, after hit protection. */
	UFUNCTION(BlueprintPure, Category = "Slime|Player")
	float GetTotalDamageTaken() const { return TotalDamageTaken; }

	/** Called by the player AttributeSet with the actual negative health delta. */
	void RecordDamageTaken(float Amount);

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

	/** M6 release-triggered charged shot. */
	bool PerformChargedShot(
		float ChargeAlpha,
		FVector& OutImpactPoint,
		AActor*& OutHitActor,
		bool& bOutWeakpoint);

	float GetChargeAlpha() const;
	bool IsCharging() const { return bCharging; }
	float ConsumeChargeAlpha();

	void BeginReloadVisuals(float Duration);
	void EndReloadVisuals();
	bool IsReloading() const { return bReloadVisualActive; }
	float GetReloadProgress() const;

	/** Called by USlimePlayerAttributeSet when a damage effect lands. Sets the direction hook. */
	void NotifyDamagedFrom(AActor* Causer);

	/**
	 * PA-10 / PA-11 death effects: loose State.Player.Dead, movement lock and
	 * IBattleDirector::OnPlayerDied. Called by UGA_Die, or directly as the fallback when the
	 * death ability is not on the ASC.
	 *
	 * @param bCancelAbilities  true on the fallback path (nobody cancelled the running
	 *                          abilities yet); false when UGA_Die already did it, because
	 *                          cancelling from inside the death ability would end it mid-call.
	 */
	void ApplyDeathEffects(bool bCancelAbilities = false);

	/** Directional damage hint for Phase D UI. Does not move the aim centre (design 8.2). */
	UPROPERTY(BlueprintAssignable, Category = "Slime|Player")
	FSlimePlayerDamagedSignature OnPlayerDamaged;

	/**
	 * Phase D: lock movement / aiming / firing for Idle, Deploying, Result and Ended.
	 *
	 * The input component stays enabled on purpose - the pause key has to keep working while the
	 * run is frozen - so the gate lives in the input handlers and in Tick's hold-to-fire.
	 */
	void SetRunInputBlocked(bool bBlocked);

	UFUNCTION(BlueprintPure, Category = "Slime|Player")
	bool IsRunInputBlocked() const { return bInputBlocked; }

	/**
	 * Phase D: the run is over. Cancels the running abilities (that is PA-10's run-end half:
	 * a reload in flight must not survive into the settlement screen), drops Controllable and
	 * raises State.Player.Result so every ActivationBlockedTags gate closes at once.
	 */
	void EnterResultState();

	/** The pause key was pressed. The UI layer owns what "pause" means (Phase D / PD-13). */
	UPROPERTY(BlueprintAssignable, Category = "Slime|Player")
	FSlimePauseRequestedSignature OnPauseRequested;

        UPROPERTY(BlueprintAssignable, Category = "Slime|Player")
        FSlimeGrenadeInventoryChangedSignature OnGrenadeInventoryChanged;

        UPROPERTY(BlueprintAssignable, Category = "Slime|Player")
        FSlimeGrenadeSelectionChangedSignature OnGrenadeSelectionChanged;

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
	UFUNCTION() void OnSprintPressed();
	UFUNCTION() void OnSprintReleased();
	UFUNCTION() void OnJumpPressed();
	UFUNCTION() void OnSwitchShoulderPressed();
	UFUNCTION() void OnPausePressed();
        UFUNCTION() void SelectAssimilationGrenade();
        UFUNCTION() void SelectTornadoGrenade();
        UFUNCTION() void SelectFactionA();
        UFUNCTION() void SelectFactionB();
        UFUNCTION() void SelectFactionC();
        UFUNCTION() void ThrowSelectedGrenade();

	bool CanSelectFaction(ESlimeFaction Faction) const;

	// -- GAS --
	void InitAbilitySystem();
	void ApplyRunConfig();
	void SyncHealthMirror();

	UFUNCTION() void HandleMirrorDeath();

	/** Aim assist cone search; returns CameraForward unchanged when not aiming or nothing is close. */
	FVector GetAimDirectionWithAssist(const FVector& CameraLocation, const FVector& CameraForward, float MaxRange, AActor** OutAssistTarget = nullptr) const;

	/** M3: push nearby mass 1-3 ecology bodies without damage or ammo. */
	void ApplyBodyPushToNearbyEcology();
	bool CanStartCharge() const;

	bool HasClearShot(const FVector& From, const AActor* Target) const;

	const USlimeRunConfig* GetRunConfig() const;
	void UpdateMovementSpeed();
	void ApplyAimCamera(bool bAim);
	void ApplyShoulderOffset();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY()
	TObjectPtr<USlimePlayerAttributeSet> Attributes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|Health")
	TObjectPtr<USlimeHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|Weapon")
	TObjectPtr<USlimeWeaponComponent> Weapon;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|Weapon")
	TObjectPtr<USlimeAimAssistComponent> AimAssist;

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

	bool bCharging = false;
	float ChargeStartTime = 0.f;
	float PendingChargeAlpha = 0.f;
	bool bReloadVisualActive = false;
	float ReloadVisualStart = 0.f;
	float ReloadVisualDuration = 0.f;

	/** Right mouse held: enables aim assist and strafe facing. */
	bool bIsAiming = false;

	bool bIsSprinting = false;
	bool bIsLeftShoulder = false;
	float CachedWalkSpeed = 0.f;
	float CachedSprintSpeed = 0.f;
	float CachedAimSpeed = 0.f;
	float CachedNormalBoomLength = 0.f;
	float CachedAimBoomLength = 0.f;
	float CachedNormalFOV = 0.f;
	float CachedAimFOV = 0.f;
	FVector CachedShoulderOffset = FVector::ZeroVector;

	/** Previous mirrored health, used to detect "just took damage" for hit protection. */
	float LastMirroredHealth = 0.f;
	bool bHasMirroredHealth = false;

	/** Guard so the death sequence (ability + fallback) only ever runs once. */
	bool bDeathHandled = false;

	/** True while the run is not in Running (Phase D). Gates the input handlers, not the component. */
	bool bInputBlocked = false;

	/** M3: last world-space movement intent, used when collision has already zeroed velocity. */
	FVector LastMoveDirection = FVector::ZeroVector;
	float LastMoveInputTime = -100.f;

        ESlimeGrenadeType SelectedGrenadeType = ESlimeGrenadeType::Assimilation;
        ESlimeFaction SelectedGrenadeFaction = ESlimeFaction::A;
        int32 AssimilationGrenades = 3;
        int32 TornadoGrenades = 3;
        float GrenadeCooldownRemaining = 0.f;
	float TotalDamageTaken = 0.f;

	// -- Presentation: hurt indicator (U-C7) --

	/**
	 * Screen-edge red flash, driven through a post process component attached to this character.
	 *
	 * A component (rather than a PostProcessVolume per level) means every map gets the effect
	 * without anyone remembering to place an actor; the blendable material is assigned on the BP.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime|Presentation")
	TObjectPtr<UPostProcessComponent> HurtPostProcess;

	/** Overwrites the material used for the flash; empty keeps the fallback path hard-coded below. */
	UPROPERTY(EditDefaultsOnly, Category = "Slime|Presentation")
	TObjectPtr<UMaterialInterface> HurtIndicatorMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HurtIndicatorMID;

	/** Current red-screen intensity, 0..1. */
	float HurtIndicatorAlpha = 0.f;

	/** 0..1/seconds decay applied after a hit; the low-health floor is separate. */
	float HurtIndicatorDecay = 0.f;

	UFUNCTION()
	void HandlePlayerDamagedForIndicator(FVector DamageDirection);

	void UpdateHurtIndicator(float DeltaSeconds);
};
