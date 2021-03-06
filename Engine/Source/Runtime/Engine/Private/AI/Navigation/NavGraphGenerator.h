// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "AI/NavDataGenerator.h"

/**
 * Class that handles generation of the ANavigationGraph data
 */
class FNavGraphGenerator : public FNavDataGenerator
{
public:
	FNavGraphGenerator(class ANavigationGraph* InDestNavGraph);
	virtual ~FNavGraphGenerator();

private:
	/** Prevent copying. */
	FNavGraphGenerator(FNavGraphGenerator const& NoCopy) { check(0); };
	FNavGraphGenerator& operator=(FNavGraphGenerator const& NoCopy) { check(0); return *this; }

public:
	//----------------------------------------------------------------------//
	// FNavDataGenerator overrides
	//----------------------------------------------------------------------//
	virtual bool IsBuildInProgress(bool bCheckDirtyToo = false) const override;

private:
	// Performs initial setup of member variables so that generator is ready to do its thing from this point on
	void Init();
	void CleanUpIntermediateData();
	void UpdateBuilding();

private:
	/** Bounding geometry definition. */
	TArray<AVolume const*> InclusionVolumes;

	FCriticalSection GraphChangingLock;

	class ANavigationGraph* DestNavGraph;

	uint32 bInitialized:1;
};
