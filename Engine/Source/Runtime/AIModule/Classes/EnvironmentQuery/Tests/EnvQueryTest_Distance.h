// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvQueryTest_Distance.generated.h"

UENUM()
namespace EEnvTestDistance
{
	enum Type
	{
		Distance3D,
		Distance2D,
		DistanceZ,
	};
}

UCLASS()
class UEnvQueryTest_Distance : public UEnvQueryTest
{
	GENERATED_UCLASS_BODY()

	/** testing mode */
	UPROPERTY(EditDefaultsOnly, Category=Distance)
	TEnumAsByte<EEnvTestDistance::Type> TestMode;

	/** context */
	UPROPERTY(EditDefaultsOnly, Category=Distance)
	TSubclassOf<class UEnvQueryContext> DistanceTo;

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

	virtual FString GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;
};
