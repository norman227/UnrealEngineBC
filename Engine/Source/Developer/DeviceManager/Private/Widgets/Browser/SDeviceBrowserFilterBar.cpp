// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "DeviceManagerPrivatePCH.h"
#include "SSearchBox.h"


#define LOCTEXT_NAMESPACE "SDeviceBrowserFilterBar"


/* SSessionBrowserFilterBar structors
 *****************************************************************************/

SDeviceBrowserFilterBar::~SDeviceBrowserFilterBar( )
{
	if (Filter.IsValid())
	{
		Filter->OnFilterReset().RemoveAll(this);
	}	
}


/* SSessionBrowserFilterBar interface
 *****************************************************************************/

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SDeviceBrowserFilterBar::Construct( const FArguments& InArgs, FDeviceBrowserFilterRef InFilter )
{
	Filter = InFilter;

	ChildSlot
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.Padding(6.0f, 0.0f, 6.0f, 0.0f)
			[
				// platform filter
				SNew(SComboButton)
					.ComboButtonStyle(FEditorStyle::Get(), "ToolbarComboButton")
					.ForegroundColor(FLinearColor::White)
					.ButtonContent()
					[
						SNew(STextBlock)
						.TextStyle(FEditorStyle::Get(), "Launcher.Filters.Text")
						.Text(LOCTEXT("PlatformFiltersComboButtonText", "Platform Filters"))
					]
					.ContentPadding(0.0f)
					.MenuContent()
					[
						SAssignNew(PlatformListView, SListView<TSharedPtr<FDeviceBrowserFilterEntry> >)
							.ItemHeight(24.0f)
							.ListItemsSource(&Filter->GetFilteredPlatforms())
							.OnGenerateRow(this, &SDeviceBrowserFilterBar::HandlePlatformListViewGenerateRow)
					]
			]

		+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Top)
			[
				// search box
				SAssignNew(FilterStringTextBox, SSearchBox)
				.HintText(LOCTEXT("SearchBoxHint", "Search devices"))
				.OnTextChanged(this, &SDeviceBrowserFilterBar::HandleFilterStringTextChanged)
			]

	];

	Filter->OnFilterReset().AddSP(this, &SDeviceBrowserFilterBar::HandleFilterReset);
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION


/* SSessionBrowserFilterBar callbacks
 *****************************************************************************/

void SDeviceBrowserFilterBar::HandleFilterReset( )
{
	FilterStringTextBox->SetText(Filter->GetDeviceSearchText());
	PlatformListView->RequestListRefresh();
}


void SDeviceBrowserFilterBar::HandleFilterStringTextChanged( const FText& NewText )
{
	Filter->SetDeviceSearchString(NewText);
}


void SDeviceBrowserFilterBar::HandlePlatformListRowCheckStateChanged(ESlateCheckBoxState::Type CheckState, TSharedPtr<FDeviceBrowserFilterEntry> PlatformEntry)
{
	Filter->SetPlatformEnabled(PlatformEntry->PlatformName, CheckState == ESlateCheckBoxState::Checked);
}


ESlateCheckBoxState::Type SDeviceBrowserFilterBar::HandlePlatformListRowIsChecked(TSharedPtr<FDeviceBrowserFilterEntry> PlatformEntry) const
{
	if (Filter->IsPlatformEnabled(PlatformEntry->PlatformName))
	{
		return ESlateCheckBoxState::Checked;
	}

	return ESlateCheckBoxState::Unchecked;
}


FString SDeviceBrowserFilterBar::HandlePlatformListRowText(TSharedPtr<FDeviceBrowserFilterEntry> PlatformEntry) const
{
	return FString::Printf(TEXT("%s (%i)"), *(PlatformEntry->PlatformName), Filter->GetServiceCountPerPlatform(PlatformEntry->PlatformName));
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedRef<ITableRow> SDeviceBrowserFilterBar::HandlePlatformListViewGenerateRow(TSharedPtr<FDeviceBrowserFilterEntry> PlatformEntry, const TSharedRef<STableViewBase>& OwnerTable)
{
	const PlatformInfo::FPlatformInfo* const PlatformInfo = PlatformInfo::FindPlatformInfo(PlatformEntry->PlatformLookup);

	return SNew(STableRow<TSharedPtr<FString> >, OwnerTable)
		.Content()
		[
			SNew(SCheckBox)
			.IsChecked(this, &SDeviceBrowserFilterBar::HandlePlatformListRowIsChecked, PlatformEntry)
			.Padding(FMargin(6.0, 2.0))
			.OnCheckStateChanged(this, &SDeviceBrowserFilterBar::HandlePlatformListRowCheckStateChanged, PlatformEntry)
			.Content()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(24)
					.HeightOverride(24)
					[
						SNew(SImage)
						.Image((PlatformInfo) ? FEditorStyle::GetBrush(PlatformInfo->GetIconStyleName(PlatformInfo::EPlatformIconSize::Normal)) : FStyleDefaults::GetNoBrush())
					]
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &SDeviceBrowserFilterBar::HandlePlatformListRowText, PlatformEntry)
				]
			]
		];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION


#undef LOCTEXT_NAMESPACE
