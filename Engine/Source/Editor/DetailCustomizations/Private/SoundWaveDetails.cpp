// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "SoundWaveDetails.h"

TSharedRef<IDetailCustomization> FSoundWaveDetails::MakeInstance()
{
	return MakeShareable(new FSoundWaveDetails);
}

void FSoundWaveDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	if (!GetDefault<UEditorExperimentalSettings>()->bShowAudioStreamingOptions)
	{
		DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(USoundWave, bStreaming));
		DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(USoundWave, StreamingPriority));
	}
}
