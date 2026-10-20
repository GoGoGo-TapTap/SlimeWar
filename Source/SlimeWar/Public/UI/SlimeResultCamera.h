// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Camera/CameraActor.h"
#include "Core/SlimeWarCoreTypes.h"
#include "CoreMinimal.h"
#include "SlimeResultCamera.generated.h"

class ASlimeRunGameState;
class USceneCaptureComponent2D;
class UTexture;
class UTextureRenderTarget2D;

/**
 * Settlement photo camera.
 *
 * A static rig placed in the level: when the run reaches the Result phase it takes one picture,
 * and the settlement screen shows that picture through GetPhoto().
 *
 * Deliberately independent from the result orbit sequence (LS_Result): a SceneCapture renders from
 * its own transform, not from the player controller's view target, so the orbit can play and the
 * photo is still taken. That also means one level needs ONE camera whose framing is authored on
 * purpose, instead of trying to match the last frame of a shared sequence (two maps cannot share
 * one camera move).
 *
 * No photo is a supported state: GetPhoto() returns null and the WBP falls back to T_Result_BG.
 */
UCLASS()
class SLIMEWAR_API ASlimeResultCamera : public ACameraActor
{
	GENERATED_BODY()

public:
	ASlimeResultCamera();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** First photo camera in the world, or null. Warns when a level has more than one. */
	static ASlimeResultCamera* Get(const UObject* WorldContextObject);

	/** Take the picture now. Only the first call per run is honoured. */
	UFUNCTION(BlueprintCallable, Category = "Slime|Result")
	void CapturePhoto();

	/** The captured texture, or null when nothing was captured yet. */
	UFUNCTION(BlueprintPure, Category = "Slime|Result")
	UTexture* GetPhoto() const;

	UFUNCTION(BlueprintPure, Category = "Slime|Result")
	bool HasPhoto() const { return bCaptured; }

protected:
	void BindRunState();
	void HandleRunStateChanged(ESlimeRunState NewState);
	void EnsureRenderTarget();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Photo")
	TObjectPtr<USceneCaptureComponent2D> SceneCapture;

	/** Optional shared render target asset; leave empty to create one at runtime (recommended). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Photo")
	TObjectPtr<UTextureRenderTarget2D> RenderTargetOverride;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Photo", meta = (ClampMin = "64"))
	int32 CaptureWidth = 1024;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Photo", meta = (ClampMin = "64"))
	int32 CaptureHeight = 640;

	/** Delay after entering Result, so death dissolve / splatter effects have settled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Photo", meta = (ClampMin = "0.0"))
	float PhotoDelay = 0.3f;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> PhotoTarget;

	TWeakObjectPtr<ASlimeRunGameState> BoundGameState;

	FTimerHandle PhotoTimerHandle;

	bool bCaptured = false;
};
