// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "SSafeZone.h"


void SSafeZone::Construct(const FArguments& InArgs)
{
	SBox::Construct(SBox::FArguments()
		.HAlign(InArgs._HAlign)
		.VAlign(InArgs._VAlign)
		.Padding(this, &SSafeZone::GetSafeZonePadding)
		[
			InArgs._Content.Widget
		]
	);
	
	IsTitleSafe = InArgs._IsTitleSafe;
	Padding = InArgs._Padding;
}

FMargin SSafeZone::GetSafeZonePadding() const
{
	// @todo: should we have a function that just returns the safe area size?
	FDisplayMetrics Metrics;
	FSlateApplication::Get().GetDisplayMetrics(Metrics);

	// return either the TitleSafe or the ActionSafe size, added to the user padding
	return Padding.Get() + (IsTitleSafe ? FMargin(Metrics.TitleSafePaddingSize.X, Metrics.TitleSafePaddingSize.Y) :
		FMargin(Metrics.ActionSafePaddingSize.X, Metrics.ActionSafePaddingSize.Y));
}
