// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionConstant4Vector.generated.h"

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionConstant4Vector : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	float R_DEPRECATED;

	UPROPERTY()
	float G_DEPRECATED;

	UPROPERTY()
	float B_DEPRECATED;

	UPROPERTY()
	float A_DEPRECATED;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionConstant4Vector)
	FLinearColor Constant;

	// Begin UObject interface.
	virtual void PostLoad() override;
	// End UObject interface.

	// Begin UMaterialExpression Interface
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#if WITH_EDITOR
	virtual FString GetDescription() const override;
	virtual uint32 GetOutputType(int32 OutputIndex) override {return MCT_Float4;}
#endif // WITH_EDITOR
	// End UMaterialExpression Interface
};



