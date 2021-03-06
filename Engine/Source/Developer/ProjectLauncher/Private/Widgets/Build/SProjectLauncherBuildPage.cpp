// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "ProjectLauncherPrivatePCH.h"
#include "SExpandableArea.h"


#define LOCTEXT_NAMESPACE "SProjectLauncherBuildPage"


/* SProjectLauncherCookPage structors
 *****************************************************************************/

SProjectLauncherBuildPage::~SProjectLauncherBuildPage( )
{
	if (Model.IsValid())
	{
		Model->OnProfileSelected().RemoveAll(this);
	}
}


/* SProjectLauncherCookPage interface
 *****************************************************************************/

void SProjectLauncherBuildPage::Construct( const FArguments& InArgs, const FProjectLauncherModelRef& InModel )
{
	Model = InModel;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
					.FillWidth(1.0)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
							.Text(LOCTEXT("BuildText", "Do you wish to build?"))
					]

				+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(8.0, 0.0, 0.0, 0.0)
					[
						// build mode check box
						SNew(SCheckBox)
						.IsChecked(this, &SProjectLauncherBuildPage::HandleBuildIsChecked)
						.OnCheckStateChanged(this, &SProjectLauncherBuildPage::HandleBuildCheckedStateChanged)
					]
			]

		+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 3, 0, 3)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.Visibility(this, &SProjectLauncherBuildPage::ShowBuildConfiguration)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SProjectLauncherFormLabel)
						.ErrorToolTipText(NSLOCTEXT("SProjectLauncherBuildValidation", "NoBuildConfigurationSelectedError", "A Build Configuration must be selected."))
						.ErrorVisibility(this, &SProjectLauncherBuildPage::HandleValidationErrorIconVisibility, ELauncherProfileValidationErrors::NoBuildConfigurationSelected)
						.LabelText(LOCTEXT("ConfigurationComboBoxLabel", "Build Configuration:"))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						// build configuration selector
						SNew(SProjectLauncherBuildConfigurationSelector)
						.Font(FCoreStyle::Get().GetFontStyle(TEXT("NormalFont")))
						.OnConfigurationSelected(this, &SProjectLauncherBuildPage::HandleBuildConfigurationSelectorConfigurationSelected)
						.Text(this, &SProjectLauncherBuildPage::HandleBuildConfigurationSelectorText)
					]
				]
			]

        + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 8.0f, 0.0f, 0.0f)
            [
                SNew(SExpandableArea)
                .AreaTitle(LOCTEXT("AdvancedAreaTitle", "Advanced Settings"))
                .InitiallyCollapsed(true)
                .Padding(8.0)
                .BodyContent()
                [
                    SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("GenDSYMText", "Generate DSYM"))
                        .IsEnabled( this, &SProjectLauncherBuildPage::HandleGenDSYMButtonEnabled )
                        .OnClicked( this, &SProjectLauncherBuildPage::HandleGenDSYMClicked )
                    ]
                ]
            ]

		+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SProjectLauncherCookedPlatforms, InModel)
					.Visibility(this, &SProjectLauncherBuildPage::HandleBuildPlatformVisibility)
			]
	];

	Model->OnProfileSelected().AddSP(this, &SProjectLauncherBuildPage::HandleProfileManagerProfileSelected);
}


/* SProjectLauncherBuildPage implementation
 *****************************************************************************/

bool SProjectLauncherBuildPage::GenerateDSYMForProject( const FString& ProjectName, const FString& Configuration )
{
    // UAT executable
    FString ExecutablePath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() + FString(TEXT("Build")) / TEXT("BatchFiles"));
#if PLATFORM_MAC
    FString Executable = TEXT("RunUAT.command");
#elif PLATFORM_LINUX
    FString Executable = TEXT("RunUAT.sh");
#else
    FString Executable = TEXT("RunUAT.bat");
#endif

    // build UAT command line parameters
    FString CommandLine;
    CommandLine = FString::Printf(TEXT("GenerateDSYM -project=%s -config=%s"),
        *ProjectName,
        *Configuration);

    // launch the builder and monitor its process
    FProcHandle ProcessHandle = FPlatformProcess::CreateProc(*(ExecutablePath / Executable), *CommandLine, false, false, false, NULL, 0, *ExecutablePath, NULL);

	return ProcessHandle.Close();
}


/* SProjectLauncherBuildPage callbacks
 *****************************************************************************/

void SProjectLauncherBuildPage::HandleBuildCheckedStateChanged( ESlateCheckBoxState::Type CheckState )
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		SelectedProfile->SetBuildGame(CheckState == ESlateCheckBoxState::Checked);
	}
}


ESlateCheckBoxState::Type SProjectLauncherBuildPage::HandleBuildIsChecked() const
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		if (SelectedProfile->IsBuilding())
		{
			return ESlateCheckBoxState::Checked;
		}
	}

	return ESlateCheckBoxState::Unchecked;
}


void SProjectLauncherBuildPage::HandleProfileManagerProfileSelected( const ILauncherProfilePtr& SelectedProfile, const ILauncherProfilePtr& PreviousProfile )
{
	// reload settings
}


EVisibility SProjectLauncherBuildPage::HandleBuildPlatformVisibility( ) const
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		if (SelectedProfile->GetCookMode() == ELauncherProfileCookModes::DoNotCook && SelectedProfile->GetDeploymentMode() == ELauncherProfileDeploymentModes::DoNotDeploy)
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Collapsed;
}


FReply SProjectLauncherBuildPage::HandleGenDSYMClicked()
{
    ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

    if(SelectedProfile.IsValid())
    {
        if (!SelectedProfile->HasValidationError(ELauncherProfileValidationErrors::NoProjectSelected))
        {
            FString ProjectName = SelectedProfile->GetProjectName();
            EBuildConfigurations::Type ProjectConfig = SelectedProfile->GetBuildConfiguration();

            GenerateDSYMForProject( ProjectName, EBuildConfigurations::ToString(ProjectConfig) );
        }
    }

    return FReply::Handled();
}


bool SProjectLauncherBuildPage::HandleGenDSYMButtonEnabled() const
{
    ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

    if (SelectedProfile.IsValid())
    {
        if (!SelectedProfile->HasValidationError(ELauncherProfileValidationErrors::NoProjectSelected))
        {
            return true;
        }
    }

    return false;
}


EVisibility SProjectLauncherBuildPage::ShowBuildConfiguration() const
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid() && SelectedProfile->IsBuilding())
	{
		return EVisibility::Visible;
	}
	else
	{
		return EVisibility::Collapsed;
	}
}


void SProjectLauncherBuildPage::HandleBuildConfigurationSelectorConfigurationSelected(EBuildConfigurations::Type Configuration)
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		SelectedProfile->SetBuildConfiguration(Configuration);
	}
}


FString SProjectLauncherBuildPage::HandleBuildConfigurationSelectorText() const
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		return EBuildConfigurations::ToString(SelectedProfile->GetBuildConfiguration());
	}

	return FString();
}


EVisibility SProjectLauncherBuildPage::HandleValidationErrorIconVisibility(ELauncherProfileValidationErrors::Type Error) const
{
	ILauncherProfilePtr SelectedProfile = Model->GetSelectedProfile();

	if (SelectedProfile.IsValid())
	{
		if (SelectedProfile->HasValidationError(Error))
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Hidden;
}


#undef LOCTEXT_NAMESPACE
