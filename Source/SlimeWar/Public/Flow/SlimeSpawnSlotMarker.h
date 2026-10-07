// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ArrowComponent.h"
#include "Core/SlimeWarCoreTypes.h"
#include "SlimeSpawnSlotMarker.generated.h"

/**
 * One draggable spawn slot handle, placed in the level under an ASpawnPoint.
 *
 * This is pure data plus an editor visual: it holds no logic and the runtime never reads it. The
 * spawner always reads DA_SpawnLayout, so the asset stays the single runtime source of truth and
 * these markers are only the editing surface (moved, rotated and exported by SlimeWarEditor).
 *
 * bIsEditorOnly keeps them out of packaged builds entirely, and UArrowComponent already sets
 * bHiddenInGame, so a PIE session shows nothing either.
 */
UCLASS(ClassGroup = (SlimeWar), meta = (BlueprintSpawnableComponent))
class SLIMEWAR_API USlimeSpawnSlotMarker : public UArrowComponent
{
	GENERATED_BODY()

public:
	USlimeSpawnSlotMarker();

	/** Which slot list this handle belongs to. Drives the arrow colour. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	ESlimeSlotRole Role = ESlimeSlotRole::Normal;

	/** Order inside its role, so the exported array order is explicit rather than "whatever order". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	int32 SlotIndex = 0;
};

/**
 * Editor-only icon for the spawn point itself.
 *
 * The centre of a point IS the anchor's transform, so this component exists purely to make the
 * anchor visible and clickable in the viewport (and to give the editor module a component to hang
 * the activity radius visualisation on).
 */
UCLASS(ClassGroup = (SlimeWar), meta = (BlueprintSpawnableComponent))
class SLIMEWAR_API USlimeSpawnPointAnchor : public UArrowComponent
{
	GENERATED_BODY()

public:
	USlimeSpawnPointAnchor();
};
