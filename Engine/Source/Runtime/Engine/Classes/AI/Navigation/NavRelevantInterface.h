// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Interface.h"
#include "NavRelevantInterface.generated.h"

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UNavRelevantInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class INavRelevantInterface
{
	GENERATED_IINTERFACE_BODY()

	/** Prepare navigation modifiers */
	virtual void GetNavigationData(struct FNavigationRelevantData& Data) const {}

	/** Get bounds for navigation octree */
	virtual FBox GetNavigationBounds() const { return FBox(0); }

	/** Update bounds, called after moving owning actor */
	virtual void UpdateNavigationBounds() {}

	/** Are modifiers active? */
	virtual bool IsNavigationRelevant() const { return true; }

	/** Get navigation parent
	 *  Adds modifiers to existing octree node, GetNavigationBounds and IsNavigationRelevant won't be checked
	 */
	virtual UObject* GetNavigationParent() const { return NULL; }
};
