// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SlimeAimPreviewWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Core/SlimeWarLog.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/SlimeAimAssistComponent.h"
#include "Player/SlimeWarCharacter.h"

void USlimeAimPreviewWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// One MID for the ring; GetDynamicMaterial creates it on first call and returns the same one
	// afterwards, so calling it here (rather than every tick) is enough.
	if (Img_ChargeRing_Fill)
	{
		ChargeRingMID = Img_ChargeRing_Fill->GetDynamicMaterial();
		if (!ChargeRingMID)
		{
			// The brush has no material: the ring will stay static. Not fatal, so only log once.
			UE_LOG(LogSlimeWar, Warning,
				TEXT("USlimeAimPreviewWidget: Img_ChargeRing_Fill has no material, the charge ring will not animate."));
		}
	}

	// Start from a known state: the WBP may have been saved with the ring or the yellow crosshair
	// visible, which would show a permanent full ring before the first shot.
	if (ChargeRingMID)
	{
		ChargeRingMID->SetScalarParameterValue(ChargePercentParameter, 0.f);
	}
	if (Img_ChargeRing_Fill)
	{
		Img_ChargeRing_Fill->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Img_Crosshair_Weakpoint)
	{
		Img_Crosshair_Weakpoint->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Img_Crosshair_Default)
	{
		Img_Crosshair_Default->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (Text_ChargePercent)
	{
		Text_ChargePercent->SetText(FText::AsPercent(0.f));
	}
}

void USlimeAimPreviewWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	RefreshAimState();
}

ASlimeWarCharacter* USlimeAimPreviewWidget::ResolveCharacter()
{
	if (ASlimeWarCharacter* Cached = CachedCharacter.Get())
	{
		return Cached;
	}

	ASlimeWarCharacter* Character = Cast<ASlimeWarCharacter>(GetOwningPlayerPawn());
	if (Character)
	{
		CachedCharacter = Character;
	}

	return Character;
}

void USlimeAimPreviewWidget::RefreshAimState()
{
	ASlimeWarCharacter* Character = ResolveCharacter();
	if (!Character)
	{
		return;
	}

	const bool bCharging = Character->IsCharging();
	const float ChargeAlpha = bCharging ? FMath::Clamp(Character->GetChargeAlpha(), 0.f, 1.f) : 0.f;
	const int32 ChargePercent = FMath::RoundToInt(ChargeAlpha * 100.f);

	bool bWeakpoint = false;
	if (const USlimeAimAssistComponent* AimAssist = Character->GetAimAssistComponent())
	{
		bWeakpoint = AimAssist->IsWeakpointLocked();
	}

	// -- charge ring --
	if (ChargeRingMID && !FMath::IsNearlyEqual(ChargeAlpha, LastChargeAlpha))
	{
		ChargeRingMID->SetScalarParameterValue(ChargePercentParameter, ChargeAlpha);
		LastChargeAlpha = ChargeAlpha;
	}

	if (Text_ChargePercent && ChargePercent != LastChargePercent)
	{
		Text_ChargePercent->SetText(FText::AsPercent(ChargeAlpha));
		LastChargePercent = ChargePercent;
	}

	// Hidden the moment charging stops, so releasing the button cannot leave a full ring behind.
	if (Img_ChargeRing_Fill && bCharging != bLastCharging)
	{
		Img_ChargeRing_Fill->SetVisibility(
			bCharging ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		bLastCharging = bCharging;
	}

	// -- crosshairs --
	if (bWeakpoint != bLastWeakpoint)
	{
		if (Img_Crosshair_Weakpoint)
		{
			Img_Crosshair_Weakpoint->SetVisibility(
				bWeakpoint ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (Img_Crosshair_Default)
		{
			Img_Crosshair_Default->SetVisibility(
				bWeakpoint ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}
		bLastWeakpoint = bWeakpoint;
	}
}
