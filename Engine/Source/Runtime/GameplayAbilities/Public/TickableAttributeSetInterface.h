// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "TickableAttributeSetInterface.generated.h"

#pragma once

/** Interface for actors which can be "spotted" by a player */
UINTERFACE()
class UTickableAttributeSetInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class ITickableAttributeSetInterface
{
	GENERATED_IINTERFACE_BODY()

public:
	/**
	 * Ticks the attribute set by DeltaTime seconds
	 * 
	 * @param DeltaTime Size of the time step in seconds.
	 */
	virtual void Tick(float DeltaTime) = 0;

	/**
	* Does this attribute set need to tick?
	*
	* @return true if this attribute set should currently be ticking, false otherwise.
	*/
	virtual bool ShouldTick() const = 0;
};

