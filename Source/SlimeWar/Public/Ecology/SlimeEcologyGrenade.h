// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SlimeWarCoreTypes.h"
#include "GameFramework/Actor.h"
#include "SlimeEcologyGrenade.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ESlimeGrenadeType : uint8
{
	Assimilation,
	Tornado
};

/** Two M8 ecology grenades: faction assimilation and a temporary pulling tornado. */
UCLASS()
class SLIMEWAR_API ASlimeEcologyGrenade : public AActor
{
	GENERATED_BODY()

public:
	ASlimeEcologyGrenade();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void Launch(
		ESlimeGrenadeType InType,
		ESlimeFaction InFaction,
		const FVector& Direction);

	ESlimeGrenadeType GetGrenadeType() const { return GrenadeType; }
	ESlimeFaction GetGrenadeFaction() const { return Faction; }
	bool IsTornadoActive() const { return bTornadoActive; }

	static const TCHAR* TypeName(ESlimeGrenadeType Type);

protected:
	UFUNCTION()
	void HandleHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse,
		const FHitResult& Hit);

	void Detonate();
	void ApplyAssimilation();
	void StartTornado();
	void TickTornado(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grenade")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grenade")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grenade")
	TObjectPtr<UProjectileMovementComponent> Projectile;

	ESlimeGrenadeType GrenadeType = ESlimeGrenadeType::Assimilation;
	ESlimeFaction Faction = ESlimeFaction::A;
	float LifeRemaining = 2.f;
	float TornadoRemaining = 0.f;
	FVector TornadoCentre = FVector::ZeroVector;
	bool bDetonated = false;
	bool bTornadoActive = false;
};
