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
class USlimeWeaponComponent;
class USpringArmComponent;
struct FInputActionValue;

/**
 * Player character. GAS is fully enabled on this actor only (plan decision: layered GAS).
 *
 * Health ownership: the AttributeSet is authoritative, USlimeHealthComponent runs in proxy
 * mode and mirrors it. OnDeath is therefore the one and only death signal the rest of the
 * project ever sees. Jump was removed on purpose (out of scope for this project).
 */
UCLASS(config = Game)
class ASlimeWarCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ASlimeWarCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** Returns CameraBoom subobject. */
	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject. */
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	/** Proxy health component: mirrored from the AttributeSet, broadcasts the single OnDeath. */
	FORCEINLINE USlimeHealthComponent* GetHealthComponent() const { return Health; }

	FORCEINLINE USlimeWeaponComponent* GetWeaponComponent() const { return Weapon; }

protected:
	virtual void BeginPlay() override;
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// -- Movement / camera input --
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

	/** Stub: routes the ability input actions to the ASC once Phase A creates the assets. */
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

	// -- Input (template mapping context, still leading with Move / Look only) --
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* LookAction;

	// -- Ability input stubs (assets are created in Phase A) --
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Ability", meta = (AllowPrivateAccess = "true"))
	UInputAction* FireAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Ability", meta = (AllowPrivateAccess = "true"))
	UInputAction* ReloadAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Ability", meta = (AllowPrivateAccess = "true"))
	UInputAction* AimAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Ability", meta = (AllowPrivateAccess = "true"))
	UInputAction* PauseAction;
};
