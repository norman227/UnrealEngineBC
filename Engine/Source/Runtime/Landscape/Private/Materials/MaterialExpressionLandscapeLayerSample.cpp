// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "Landscape.h"
#include "MaterialCompiler.h"
#include "Materials/MaterialExpressionLandscapeLayerSample.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"

#define LOCTEXT_NAMESPACE "Landscape"


///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionLandscapeLayerSample
///////////////////////////////////////////////////////////////////////////////

UMaterialExpressionLandscapeLayerSample::UMaterialExpressionLandscapeLayerSample(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FString NAME_Landscape;
		FConstructorStatics()
			: NAME_Landscape(LOCTEXT("Landscape", "Landscape").ToString())
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	bIsParameterExpression = true;
	MenuCategories.Add(ConstructorStatics.NAME_Landscape);
}


FGuid& UMaterialExpressionLandscapeLayerSample::GetParameterExpressionId()
{
	return ExpressionGUID;
}


int32 UMaterialExpressionLandscapeLayerSample::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex)
{
	const int32 WeightCode = Compiler->StaticTerrainLayerWeight(ParameterName, Compiler->Constant(PreviewWeight));

	return WeightCode;
}


UTexture* UMaterialExpressionLandscapeLayerSample::GetReferencedTexture()
{
	return GEngine->WeightMapPlaceholderTexture;
}


void UMaterialExpressionLandscapeLayerSample::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(FString::Printf(TEXT("Sample '%s'"), *ParameterName.ToString()));
}


void UMaterialExpressionLandscapeLayerSample::GetAllParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds)
{
	if (!OutParameterNames.Contains(ParameterName))
	{
		OutParameterNames.Add(ParameterName);
		OutParameterIds.Add(ExpressionGUID);
	}
}


#undef LOCTEXT_NAMESPACE
