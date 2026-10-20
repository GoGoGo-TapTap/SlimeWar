// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeResultCamera.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Core/SlimeWarLog.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Flow/SlimeRunGameState.h"
#include "TimerManager.h"

ASlimeResultCamera::ASlimeResultCamera()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCapture"));
	SceneCapture->SetupAttachment(GetRootComponent());

	// One capture per run: capturing every frame is pure waste and costs a full scene render.
	SceneCapture->bCaptureEveryFrame = false;
	SceneCapture->bCaptureOnMovement = false;

	// Final LDR with post processing and tonemapping, so the photo matches what the player sees.
	// The HDR sources look flat in UMG because the rest of the post chain never runs.
	SceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;

	// The world is frozen during Result, but death effects would still smear the picture.
	SceneCapture->ShowFlags.SetMotionBlur(false);
}

void ASlimeResultCamera::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// SceneCapture defaults to a 90 degree FOV, which is not necessarily the camera component's.
	// Syncing here means the in-editor framing preview equals the final photo.
	if (SceneCapture && GetCameraComponent())
	{
		SceneCapture->FOVAngle = GetCameraComponent()->FieldOfView;
	}
}

void ASlimeResultCamera::BeginPlay()
{
	Super::BeginPlay();

	EnsureRenderTarget();

	// The GameState is spawned by the game mode during StartPlay, so its order relative to this
	// actor's BeginPlay is not guaranteed: wait one frame before binding.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &ASlimeResultCamera::BindRunState);
	}
}

void ASlimeResultCamera::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhotoTimerHandle);
	}
	PhotoTimerHandle.Invalidate();

	if (ASlimeRunGameState* RunState = BoundGameState.Get())
	{
		RunState->OnRunStateChanged.RemoveAll(this);
	}
	BoundGameState.Reset();

	Super::EndPlay(EndPlayReason);
}

void ASlimeResultCamera::BindRunState()
{
	ASlimeRunGameState* RunState = GetWorld() ? GetWorld()->GetGameState<ASlimeRunGameState>() : nullptr;
	if (!RunState)
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("ASlimeResultCamera: no ASlimeRunGameState; the settlement photo will not be taken."));
		return;
	}

	RunState->OnRunStateChanged.AddUObject(this, &ASlimeResultCamera::HandleRunStateChanged);
	BoundGameState = RunState;

	// Rare but possible: the camera is spawned after the phase already reached Result.
	if (RunState->GetRunState() == ESlimeRunState::Result)
	{
		HandleRunStateChanged(ESlimeRunState::Result);
	}
}

void ASlimeResultCamera::HandleRunStateChanged(ESlimeRunState NewState)
{
	if (bCaptured)
	{
		return;
	}

	if (NewState == ESlimeRunState::Result)
	{
		if (PhotoDelay > 0.f)
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(
					PhotoTimerHandle, this, &ASlimeResultCamera::CapturePhoto, PhotoDelay, /*bLoop=*/false);
			}
		}
		else
		{
			CapturePhoto();
		}
		return;
	}

	if (NewState == ESlimeRunState::Ended)
	{
		// Fallback: the Result capture did not happen (late spawn, interrupted timer). The
		// settlement screen is about to appear, so take it now rather than showing a black frame.
		CapturePhoto();
	}
}

void ASlimeResultCamera::CapturePhoto()
{
	if (bCaptured)
	{
		return;
	}

	EnsureRenderTarget();

	if (!SceneCapture || !SceneCapture->TextureTarget)
	{
		UE_LOG(LogSlimeWar, Warning, TEXT("ASlimeResultCamera: capture skipped, no render target."));
		return;
	}

	SceneCapture->CaptureScene();
	bCaptured = true;

	UE_LOG(LogSlimeWar, Log, TEXT("ASlimeResultCamera: settlement photo captured at %s."),
		*GetActorLocation().ToCompactString());
}

void ASlimeResultCamera::EnsureRenderTarget()
{
	if (PhotoTarget)
	{
		return;
	}

	if (RenderTargetOverride)
	{
		PhotoTarget = RenderTargetOverride;
	}
	else
	{
		PhotoTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("ResultPhotoTarget"));
		// sRGB on purpose: without it the picture is displayed too dark / washed out in UMG.
		PhotoTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8_SRGB;
		PhotoTarget->ClearColor = FLinearColor::Black;
		PhotoTarget->bAutoGenerateMips = false;
		PhotoTarget->InitAutoFormat(FMath::Max(64, CaptureWidth), FMath::Max(64, CaptureHeight));
		PhotoTarget->UpdateResourceImmediate(true);
	}

	if (SceneCapture)
	{
		SceneCapture->TextureTarget = PhotoTarget;
	}
}

UTexture* ASlimeResultCamera::GetPhoto() const
{
	return bCaptured ? PhotoTarget.Get() : nullptr;
}

ASlimeResultCamera* ASlimeResultCamera::Get(const UObject* WorldContextObject)
{
	if (!GEngine)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return nullptr;
	}

	ASlimeResultCamera* Found = nullptr;
	int32 Count = 0;

	for (TActorIterator<ASlimeResultCamera> It(World); It; ++It)
	{
		if (!Found)
		{
			Found = *It;
		}
		++Count;
	}

	if (Count > 1)
	{
		UE_LOG(LogSlimeWar, Warning,
			TEXT("ASlimeResultCamera: %d cameras in this level; the first one is used."), Count);
	}

	return Found;
}
