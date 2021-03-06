// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "GameplayAbilityTargetDataFilter.h"

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FGameplayTargetDataFilter
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

void FGameplayTargetDataFilter::InitializeFilterContext(AActor* FilterActor)
{
	SelfActor = FilterActor;
}
