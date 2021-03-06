// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ReimportPaperJsonImporterFactory.generated.h"

// Imports a paper animated sprite (and associated paper sprites and textures) from an Adobe Flash CS6 exported JSON file
UCLASS()
class UReimportPaperJsonImporterFactory : public UPaperJsonImporterFactory, public FReimportHandler
{
	GENERATED_UCLASS_BODY()

	// Begin FReimportHandler interface
	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	virtual EReimportResult::Type Reimport(UObject* Obj) override;
	// End FReimportHandler interface

	TArray<FString> ExistingSpriteNames;
	TArray< TAssetPtr<class UPaperSprite> > ExistingSprites;
	
	FString ExistingTextureName;
	UTexture2D* ExistingTexture;
};

