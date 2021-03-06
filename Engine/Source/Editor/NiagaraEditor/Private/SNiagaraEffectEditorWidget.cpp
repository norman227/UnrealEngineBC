// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorPrivatePCH.h"
#include "SNiagaraEffectEditorWidget.h"
#include "SNotificationList.h"

void SNiagaraEffectEditorWidget::Construct(const FArguments& InArgs)
{
	/*
	Commands = MakeShareable(new FUICommandList());
	IsEditable = InArgs._IsEditable;
	Appearance = InArgs._Appearance;
	TitleBarEnabledOnly = InArgs._TitleBarEnabledOnly;
	TitleBar = InArgs._TitleBar;
	bAutoExpandActionMenu = InArgs._AutoExpandActionMenu;
	ShowGraphStateOverlay = InArgs._ShowGraphStateOverlay;
	OnNavigateHistoryBack = InArgs._OnNavigateHistoryBack;
	OnNavigateHistoryForward = InArgs._OnNavigateHistoryForward;
	OnNodeSpawnedByKeymap = InArgs._GraphEvents.OnNodeSpawnedByKeymap;
	*/

	/*
	// Make sure that the editor knows about what kinds
	// of commands GraphEditor can do.
	FGraphEditorCommands::Register();

	// Tell GraphEditor how to handle all the known commands
	{
	Commands->MapAction(FGraphEditorCommands::Get().ReconstructNodes,
	FExecuteAction::CreateSP(this, &SGraphEditorImpl::ReconstructNodes),
	FCanExecuteAction::CreateSP(this, &SGraphEditorImpl::CanReconstructNodes)
	);

	Commands->MapAction(FGraphEditorCommands::Get().BreakNodeLinks,
	FExecuteAction::CreateSP(this, &SGraphEditorImpl::BreakNodeLinks),
	FCanExecuteAction::CreateSP(this, &SGraphEditorImpl::CanBreakNodeLinks)
	);

	Commands->MapAction(FGraphEditorCommands::Get().BreakPinLinks,
	FExecuteAction::CreateSP(this, &SGraphEditorImpl::BreakPinLinks, true),
	FCanExecuteAction::CreateSP(this, &SGraphEditorImpl::CanBreakPinLinks)
	);

	// Append any additional commands that a consumer of GraphEditor wants us to be aware of.
	const TSharedPtr<FUICommandList>& AdditionalCommands = InArgs._AdditionalCommands;
	if (AdditionalCommands.IsValid())
	{
	Commands->Append(AdditionalCommands.ToSharedRef());
	}

	}
	*/

	EffectObj = InArgs._EffectObj;
	//bNeedsRefresh = false;


	Viewport = SNew(SNiagaraEffectEditorViewport);


	TSharedPtr<SOverlay> OverlayWidget;
	this->ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.WidthOverride(512)
					.HeightOverride(512)
					[
						Viewport.ToSharedRef()
					]
				]
			]
			+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(OverlayWidget, SOverlay)
					/*
					// details view on the left
					+ SOverlay::Slot()
					.Expose(DetailsViewSlot)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Top)
					[
					SNew(SBorder)
					.Padding(4)
					[
					EmitterDetails.ToSharedRef()
					]
					]
					*/

					// Title bar - optional
					/*
					+ SOverlay::Slot()
					.VAlign(VAlign_Top)
					[
					InArgs._TitleBar.IsValid() ? InArgs._TitleBar.ToSharedRef() : (TSharedRef<SWidget>)SNullWidget::NullWidget
					]
					*/


					+SOverlay::Slot()
					.Padding(10)
					.VAlign(VAlign_Fill)
					[
						SAssignNew(ListView, SListView<TSharedPtr<FNiagaraSimulation> >)
						.ItemHeight(256)
						.ListItemsSource(&(this->EffectObj->Emitters))
						.OnGenerateRow(this, &SNiagaraEffectEditorWidget::OnGenerateWidgetForList)
					]

					// Bottom-right corner text indicating the type of tool
					+ SOverlay::Slot()
						.Padding(10)
						.VAlign(VAlign_Bottom)
						.HAlign(HAlign_Right)
						[
							SNew(STextBlock)
							.Visibility(EVisibility::HitTestInvisible)
							.TextStyle(FEditorStyle::Get(), "Graph.CornerText")
							.Text(FString("Niagara Effect"))
						]

					// Top-right corner text indicating read only when not simulating
					+ SOverlay::Slot()
						.Padding(20)
						.VAlign(VAlign_Top)
						.HAlign(HAlign_Right)
						[
							SNew(STextBlock)
							.Visibility(this, &SNiagaraEffectEditorWidget::ReadOnlyVisibility)
							.TextStyle(FEditorStyle::Get(), "Graph.CornerText")
							.Text(FString("Read Only"))
						]

					// Bottom-right corner text for notification list position
					+ SOverlay::Slot()
						.Padding(15.f)
						.VAlign(VAlign_Bottom)
						.HAlign(HAlign_Right)
						[
							SAssignNew(NotificationListPtr, SNotificationList)
							.Visibility(EVisibility::HitTestInvisible)
						]
				]
		];

	Viewport->SetPreviewEffect(EffectObj);
}





void SEmitterWidget::Construct(const FArguments& InArgs)
{
	CurSpawnScript = nullptr;
	CurUpdateScript = nullptr;
	Emitter = InArgs._Emitter;
	Effect = InArgs._Effect;
	CurMaterial = nullptr;

	ThumbnailPool = MakeShareable(new FAssetThumbnailPool(1, /*InAreRealTileThumbnailsAllowed=*/false));
	// Create the thumbnail handle
	MaterialThumbnail = MakeShareable(new FAssetThumbnail(CurMaterial, 64, 64, ThumbnailPool));
	ThumbnailWidget = MaterialThumbnail->MakeThumbnailWidget();
	MaterialThumbnail->GetViewportRenderTargetTexture(); // Access the texture once to trigger it to render

	RenderModuleList.Add(MakeShareable(new FString("Sprites")));
	RenderModuleList.Add(MakeShareable(new FString("Ribbon")));
	RenderModuleList.Add(MakeShareable(new FString("Trails")));
	RenderModuleList.Add(MakeShareable(new FString("Meshes")));

	ChildSlot
		.Padding(4)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.LightGroupBorder"))
			.Padding(4.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4)
				[
					NGED_SECTION_BORDER
					[
						// name and status line
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Left)
						.AutoWidth()
						[
							SNew(SCheckBox)
							.OnCheckStateChanged(this, &SEmitterWidget::OnEmitterEnabledChanged)
							.IsChecked(this, &SEmitterWidget::IsEmitterEnabled)
							.ToolTipText(FText::FromString("Toggles whether this emitter is enabled. Disabled emitters don't simulate or render."))
						]
						+ SHorizontalBox::Slot()
							.FillWidth(0.3)
							.HAlign(HAlign_Left)
							[
								SNew(SEditableText).Text(FText::FromString("Emitter"))
								.MinDesiredWidth(250)
								.Font(FEditorStyle::GetFontStyle("ContentBrowser.AssetListViewNameFontDirty"))
							]
						+ SHorizontalBox::Slot()
							.HAlign(HAlign_Right)
							.FillWidth(0.7)
							[
								SNew(STextBlock).Text(this, &SEmitterWidget::GetStatsText)
								.MinDesiredWidth(500)
							]
					]
				]

				+ SVerticalBox::Slot()
					.AutoHeight()
					[
						//------------------------------------------------------------------------------------
						// horizontal arrangement of property widgets
						SNew(SHorizontalBox)


						// Update and spawn script selectors
						//
						+SHorizontalBox::Slot()
						.Padding(4.0f)
						.FillWidth(800)
						.HAlign(HAlign_Left)
						[
							NGED_SECTION_BORDER
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.Padding(4)
								.AutoHeight()
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot()
									.HAlign(HAlign_Right)
									.VAlign(VAlign_Center)
									.AutoWidth()
									.Padding(4)
									[
										SNew(STextBlock)
										.Text(FString("Update Script"))
									]
									+ SHorizontalBox::Slot()
										.HAlign(HAlign_Right)
										.VAlign(VAlign_Center)
										[
											SAssignNew(UpdateScriptSelector, SContentReference)
											.AllowedClass(UNiagaraScript::StaticClass())
											.AssetReference(this, &SEmitterWidget::GetUpdateScript)
											.AllowSelectingNewAsset(true)
											.AllowClearingReference(true)
											.OnSetReference(this, &SEmitterWidget::OnUpdateScriptSelectedFromPicker)
											.WidthOverride(150)
										]

								]

								+ SVerticalBox::Slot()
									.Padding(4)
									.AutoHeight()
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot()
										.HAlign(HAlign_Right)
										.VAlign(VAlign_Center)
										.AutoWidth()
										.Padding(4)
										[
											SNew(STextBlock)
											.Text(FString("Spawn Script"))
										]
										+ SHorizontalBox::Slot()
											.HAlign(HAlign_Right)
											.VAlign(VAlign_Center)
											[
												SAssignNew(SpawnScriptSelector, SContentReference)
												.AllowedClass(UNiagaraScript::StaticClass())
												.AssetReference(this, &SEmitterWidget::GetSpawnScript)
												.AllowSelectingNewAsset(true)
												.AllowClearingReference(true)
												.OnSetReference(this, &SEmitterWidget::OnSpawnScriptSelectedFromPicker)
												.WidthOverride(150)
											]
									]
							]
						]


						// Spawn rate
						+SHorizontalBox::Slot()
							.AutoWidth()
							.HAlign(HAlign_Left)
							.Padding(4)
							[
								NGED_SECTION_BORDER
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot()
									.Padding(4)
									.AutoHeight()
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot()
										.HAlign(HAlign_Center)
										.VAlign(VAlign_Center)
										.AutoWidth()
										.Padding(4)
										[
											SNew(STextBlock)
											.Text(FString("Spawn rate"))
										]
										+ SHorizontalBox::Slot()
											[
												SNew(SEditableTextBox).OnTextChanged(this, &SEmitterWidget::OnSpawnRateChanged)
												.Text(this, &SEmitterWidget::GetSpawnRateText)
											]
									]
								]
							]


						// Material
						+SHorizontalBox::Slot()
							.AutoWidth()
							.HAlign(HAlign_Left)
							.Padding(4)
							[
								NGED_SECTION_BORDER
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot()
									.AutoHeight()
									.Padding(4)
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot()
										.Padding(4)
										.AutoWidth()
										[
											SNew(STextBlock)
											.Text(FString("Material"))
										]
										+ SHorizontalBox::Slot()
											.Padding(4)
											.AutoWidth()
											[
												SNew(SVerticalBox)
												+ SVerticalBox::Slot()
												.HAlign(HAlign_Left)
												.VAlign(VAlign_Center)
												.AutoHeight()
												.Padding(4)
												[
													SNew(SBox)
													.WidthOverride(64)
													.HeightOverride(64)
													[
														ThumbnailWidget.ToSharedRef()
													]
												]
												+ SVerticalBox::Slot()
													.HAlign(HAlign_Left)
													.VAlign(VAlign_Center)
													.AutoHeight()
													.Padding(4)
													[
														SNew(SContentReference)
														.AllowedClass(UMaterial::StaticClass())
														.AssetReference(this, &SEmitterWidget::GetMaterial)
														.AllowSelectingNewAsset(true)
														.AllowClearingReference(true)
														.OnSetReference(this, &SEmitterWidget::OnMaterialSelected)
														.WidthOverride(150)
													]
											]
									]
									+ SVerticalBox::Slot()
										.AutoHeight()
										.Padding(4)
										[
											SNew(SHorizontalBox)
											+ SHorizontalBox::Slot()
											.Padding(4)
											.AutoWidth()
											[
												SNew(STextBlock)
												.Text(FString("Render as"))
											]
											+ SHorizontalBox::Slot()
												.Padding(4)
												.AutoWidth()
												[
													// session combo box
													SNew(SComboBox<TSharedPtr<FString> >)
													.ContentPadding(FMargin(6.0f, 2.0f))
													.OptionsSource(&RenderModuleList)
													.OnSelectionChanged(this, &SEmitterWidget::OnRenderModuleChanged)
													//										.ToolTipText(LOCTEXT("SessionComboBoxToolTip", "Select the game session to interact with.").ToString())
													.OnGenerateWidget(this, &SEmitterWidget::GenerateRenderModuleComboWidget)
													[
														SNew(STextBlock)
														.Text(this, &SEmitterWidget::GetRenderModuleText)
													]
												]
										]
								]
							]
					]
			]
		];
}