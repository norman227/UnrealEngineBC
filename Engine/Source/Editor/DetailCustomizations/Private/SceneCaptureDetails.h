// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

class FSceneCaptureDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();
private:
	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) override;

private:
	/** The selected reflection capture */
	TWeakObjectPtr<USceneCaptureComponent> SceneCaptureComponent;

	/**
	* Gets the display state to send to a display filter check box
	*
	* @param The type of checkbox.
	* @return The desired checkbox state.
	*/
	ESlateCheckBoxState::Type OnGetDisplayCheckState(FString ShowFlagName) const;

	/** Show flag settings changed, so update the scene capture */
	void OnShowFlagCheckStateChanged(ESlateCheckBoxState::Type InNewRadioState, FString FlagName);
};
