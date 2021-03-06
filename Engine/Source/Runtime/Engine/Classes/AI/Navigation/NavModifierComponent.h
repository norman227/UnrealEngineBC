// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AI/Navigation/NavRelevantComponent.h"
#include "NavModifierComponent.generated.h"

class UNavArea;
struct FCompositeNavModifier;

UCLASS(ClassGroup = (Navigation), meta = (BlueprintSpawnableComponent), hidecategories = (Activation))
class UNavModifierComponent : public UNavRelevantComponent
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = Navigation)
	TSubclassOf<UNavArea> AreaClass;

	virtual void CalcBounds() override;
	virtual void GetNavigationData(struct FNavigationRelevantData& Data) const override;

protected:
	FVector ObstacleExtent;
};
