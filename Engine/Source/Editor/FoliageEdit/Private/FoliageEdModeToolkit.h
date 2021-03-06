// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Toolkits/BaseToolkit.h"

/**
 * Public interface to Foliage Edit mode.
 */
class FFoliageEdModeToolkit : public FModeToolkit
{
public:
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;

	/** Initializes the foliage mode toolkit */
	virtual void Init(const TSharedPtr< class IToolkitHost >& InitToolkitHost);

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual class FEdMode* GetEditorMode() const override;
	virtual TSharedPtr<class SWidget> GetInlineContent() const override;

	void PostUndo();

private:
	TSharedPtr< class SFoliageEdit > FoliageEdWidget;
};