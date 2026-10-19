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
        ESlimeGrenadeType GetSelectedGrenadeType() const { return SelectedGrenadeType; }

        UFUNCTION(BlueprintPure, Category = "Slime|Player")
        ESlimeFaction GetSelectedGrenadeFaction() const { return SelectedGrenadeFaction; }

        UFUNCTION(BlueprintPure, Category = "Slime|Player")
        int32 GetAssimilationGrenadeCount() const { return AssimilationGrenades; }

        UFUNCTION(BlueprintPure, Category = "Slime|Player")
        int32 GetTornadoGrenadeCount() const { return TornadoGrenades; }

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
	UFUNCTION() void OnPausePressed();
        UFUNCTION() void SelectAssimilationGrenade();
        UFUNCTION() void SelectTornadoGrenade();
        UFUNCTION() void SelectFactionA();
        UFUNCTION() void SelectFactionB();
        UFUNCTION() void ThrowSelectedGrenade();

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
};
