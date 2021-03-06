// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyEditorDelegates.h"

/**
 * Interface class for all detail views
 */
class IStructureDetailsView
{
public:
	virtual TSharedPtr<class SWidget> GetWidget() = 0;

	virtual void SetStructureData(TSharedPtr<struct FStructOnScope> StructData) = 0;
	virtual FOnFinishedChangingProperties& GetOnFinishedChangingPropertiesDelegate() = 0;
};
