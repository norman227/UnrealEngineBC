// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SoundDefinitions.h"

class FAssetTypeActions_DialogueVoice : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_DialogueVoice", "Dialogue Voice"); }
	virtual FColor GetTypeColor() const override { return FColor(0,0,255); }
	virtual UClass* GetSupportedClass() const override { return UDialogueVoice::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Sounds; }
	virtual bool CanFilter() override { return true; }
};