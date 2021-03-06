// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"

#include "HardwareTargetingSettingsDetails.h"
#include "HardwareTargetingModule.h"
#include "ISourceControlModule.h"
#include "SSettingsEditorCheckoutNotice.h"

#define LOCTEXT_NAMESPACE "FHardwareTargetingSettingsDetails"

TSharedRef<IDetailCustomization> FHardwareTargetingSettingsDetails::MakeInstance()
{
	return MakeShareable(new FHardwareTargetingSettingsDetails);
}

class SRequiredDefaultConfig : public SCompoundWidget
{
public:
	
	SLATE_BEGIN_ARGS(SRequiredDefaultConfig) {}

	SLATE_END_ARGS()

	~SRequiredDefaultConfig()
	{
		GetMutableDefault<UHardwareTargetingSettings>()->OnSettingChanged().RemoveAll(this);
	}
	
public:

	TMap<TWeakObjectPtr<UObject>, TSharedPtr<SRichTextBlock>> SettingRegions;

	void Construct(const FArguments& InArgs)
	{
		LastStatusUpdate = 0;
		GetMutableDefault<UHardwareTargetingSettings>()->OnSettingChanged().AddRaw(this, &SRequiredDefaultConfig::Update);

		auto ApplyNow = []{
			IHardwareTargetingModule& Module = IHardwareTargetingModule::Get();
			Module.ApplyHardwareTargetingSettings();

			return FReply::Handled();
		};

		ChildSlot
		[

			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("RestartMessage", "The following changes will be applied when this project is reopened."))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(6)
				[
					SNew(SButton)
					.Text(LOCTEXT("ApplyNow", "Apply Now"))
					.IsEnabled(this, &SRequiredDefaultConfig::CanApply)
					.OnClicked_Static(ApplyNow)
				]
			]
			
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(CheckoutNotices, SVerticalBox)
			]
		];
	}

	static EVisibility GetAnyPendingChangesVisibility()
	{
		return GetMutableDefault<UHardwareTargetingSettings>()->HasPendingChanges() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	void Initialize(IDetailLayoutBuilder& DetailBuilder, IDetailCategoryBuilder& PendingChangesCategory)
	{
		auto IsVisible = []()->EVisibility{
			auto* Settings = GetMutableDefault<UHardwareTargetingSettings>();
			return Settings->HasPendingChanges() ? EVisibility::Visible : EVisibility::Collapsed;
		};

		FText CategoryHeaderTooltip = LOCTEXT("CategoryHeaderTooltip", "List of properties modified in this project setting category");

		IHardwareTargetingModule& Module = IHardwareTargetingModule::Get();
		for (const FModifiedDefaultConfig& Settings : Module.GetPendingSettingsChanges())
		{
			TSharedRef<SRichTextBlock> EditPropertiesBlock =
				SNew(SRichTextBlock)
				.AutoWrapText(false)
				.Justification(ETextJustify::Left)
				.TextStyle(FEditorStyle::Get(), "HardwareTargets.Normal")
				.DecoratorStyleSet(&FEditorStyle::Get());

			SettingRegions.Add(Settings.SettingsObject, EditPropertiesBlock);

			FDetailWidgetRow& CategoryRow = PendingChangesCategory.AddCustomRow(Settings.CategoryHeading.ToString())
				.Visibility(TAttribute<EVisibility>::Create(&SRequiredDefaultConfig::GetAnyPendingChangesVisibility))
				.NameContent()
				[
					SNew(STextBlock)
					.Text(Settings.CategoryHeading)
					.ToolTipText(CategoryHeaderTooltip)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
				.ValueContent()
				.MaxDesiredWidth(300.0f)
				[
					EditPropertiesBlock
				];
		}

		Update();
	}

	bool CanApply() const
	{
		for (const auto& Watcher : FileWatcherWidgets)
		{
			if (!Watcher->IsUnlocked())
			{
				return false;
			}
		}

		return true;
	}

	void Update()
	{
		FileWatcherWidgets.Reset();
		CheckoutNotices->ClearChildren();

		IHardwareTargetingModule& Module = IHardwareTargetingModule::Get();
		int32 SlotIndex = 0;

		// Run thru the settings and push changes to the existing settings, as well as build a list of inis that will need to be edited
		TSet<FString> SeenConfigFiles;
		for (const FModifiedDefaultConfig& Settings : Module.GetPendingSettingsChanges())
		{
			if (!Settings.SettingsObject.IsValid())
			{
				continue;
			}

			SettingRegions.FindChecked(Settings.SettingsObject)->SetText(Settings.Description);

			if (!Settings.SettingsObject->GetClass()->HasAnyClassFlags(CLASS_Config | CLASS_DefaultConfig))
			{
				continue;
			}
			

			FString ConfigFile = FPaths::ConvertRelativePathToFull(Settings.SettingsObject->GetDefaultConfigFilename());

			if (!SeenConfigFiles.Contains(ConfigFile))
			{
				SeenConfigFiles.Add(ConfigFile);

				TSharedRef<SSettingsEditorCheckoutNotice> FileWatcherWidget =
					SNew(SSettingsEditorCheckoutNotice)
					.ConfigFilePath(ConfigFile);
				FileWatcherWidgets.Add(FileWatcherWidget);

				CheckoutNotices->AddSlot()
				.Padding(FMargin(0.f, 0.f, 12.f, 5.f))
				[
					FileWatcherWidget
				];
				SlotIndex++;
			}
		}
	}

	TArray<TSharedPtr<SSettingsEditorCheckoutNotice>> FileWatcherWidgets;

	TSharedPtr<SVerticalBox> CheckoutNotices;

	double LastStatusUpdate;
};

void FHardwareTargetingSettingsDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{	
	IDetailCategoryBuilder& HardwareTargetingCategory = DetailBuilder.EditCategory(TEXT("Target Hardware"));
	IDetailCategoryBuilder& PendingChangesCategory = DetailBuilder.EditCategory(TEXT("Pending Changes"));

	auto AnyPendingChangesVisible = []()->EVisibility{
		auto* Settings = GetMutableDefault<UHardwareTargetingSettings>();
		return Settings->HasPendingChanges() ? EVisibility::Visible : EVisibility::Collapsed;
	};
	auto NoPendingChangesVisible = []()->EVisibility{
		auto* Settings = GetMutableDefault<UHardwareTargetingSettings>();
		return Settings->HasPendingChanges() ? EVisibility::Collapsed : EVisibility::Visible;
	};

	TSharedRef<SRequiredDefaultConfig> ConfigWidget = SNew(SRequiredDefaultConfig);
	IHardwareTargetingModule& HardwareTargeting = IHardwareTargetingModule::Get();
	PendingChangesCategory.AddCustomRow(FString())
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		[
			SNew(SBox)
			.Visibility_Static(AnyPendingChangesVisible)
			[
				ConfigWidget
			]
		]
		+SVerticalBox::Slot()
		[
			SNew(SBox)
			.Visibility_Static(NoPendingChangesVisible)
			[
				SNew(STextBlock)
				.Font(DetailBuilder.GetDetailFont())
				.Text(LOCTEXT("NoPendingChangesMessage", "There are no pending settings changes."))
			]
		]
	];

	ConfigWidget->Initialize(DetailBuilder, PendingChangesCategory);

	TSharedPtr<SWidget> HardwareClassCombo, GraphicsPresetCombo;

	// Setup the hardware class combo
	{
		auto PropertyName = GET_MEMBER_NAME_CHECKED(UHardwareTargetingSettings, TargetedHardwareClass);
		DetailBuilder.HideProperty(PropertyName);

		TSharedRef<IPropertyHandle> Property = DetailBuilder.GetProperty(PropertyName);
		auto SetPropertyValue = [](EHardwareClass::Type NewValue, TSharedRef<IPropertyHandle> Property){
			Property->SetValue(uint8(NewValue));
		};
		auto GetPropertyValue = [](TSharedRef<IPropertyHandle> Property){
			uint8 Value = 0;
			Property->GetValue(Value);
			return EHardwareClass::Type(Value);
		};
		
		HardwareClassCombo = HardwareTargeting.MakeHardwareClassTargetCombo(
			FOnHardwareClassChanged::CreateStatic(SetPropertyValue, Property),
			TAttribute<EHardwareClass::Type>::Create(TAttribute<EHardwareClass::Type>::FGetter::CreateStatic(GetPropertyValue, Property))
		);
	}

	// Setup the graphics preset combo
	{
		auto PropertyName = GET_MEMBER_NAME_CHECKED(UHardwareTargetingSettings, DefaultGraphicsPerformance);
		DetailBuilder.HideProperty(PropertyName);

		TSharedRef<IPropertyHandle> Property = DetailBuilder.GetProperty(PropertyName);
		auto SetPropertyValue = [](EGraphicsPreset::Type NewValue, TSharedRef<IPropertyHandle> Property){
			Property->SetValue(uint8(NewValue));
		};
		auto GetPropertyValue = [](TSharedRef<IPropertyHandle> Property){
			uint8 Value = 0;
			Property->GetValue(Value);
			return EGraphicsPreset::Type(Value);
		};
		GraphicsPresetCombo = HardwareTargeting.MakeGraphicsPresetTargetCombo(
			FOnGraphicsPresetChanged::CreateStatic(SetPropertyValue, Property),
			TAttribute<EGraphicsPreset::Type>::Create(TAttribute<EGraphicsPreset::Type>::FGetter::CreateStatic(GetPropertyValue, Property))
		);
	}

	HardwareTargetingCategory.AddCustomRow(LOCTEXT("HardwareTargetingOption", "Targeted Hardware:").ToString())
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("OptimizeProjectFor", "Optimize project settings for:"))
		.Font(DetailBuilder.GetDetailFont())
	]
	.ValueContent()
	.MaxDesiredWidth(0)
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.Padding(FMargin(10.f, 0.f))
		.AutoWidth()
		[
			HardwareClassCombo.ToSharedRef()
		]

		+ SHorizontalBox::Slot()
		.Padding(FMargin(0.f, 0.f, 10.f, 0.f))
		.AutoWidth()
		[
			GraphicsPresetCombo.ToSharedRef()
		]
	];
}

#undef LOCTEXT_NAMESPACE
