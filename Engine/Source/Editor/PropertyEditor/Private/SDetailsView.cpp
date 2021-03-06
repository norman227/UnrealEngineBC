// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "PropertyEditorPrivatePCH.h"
#include "AssetSelection.h"
#include "PropertyNode.h"
#include "ItemPropertyNode.h"
#include "CategoryPropertyNode.h"
#include "ObjectPropertyNode.h"
#include "ScopedTransaction.h"
#include "AssetThumbnail.h"
#include "SDetailNameArea.h"
#include "IPropertyUtilities.h"
#include "PropertyEditorHelpers.h"
#include "PropertyEditor.h"
#include "PropertyDetailsUtilities.h"
#include "SPropertyEditorEditInline.h"
#include "ObjectEditorUtils.h"
#include "SColorPicker.h"
#include "SSearchBox.h"


#define LOCTEXT_NAMESPACE "SDetailsView"


SDetailsView::~SDetailsView()
{
	SaveExpandedItems();
};

/**
 * Constructs the widget
 *
 * @param InArgs   Declaration from which to construct this widget.              
 */
void SDetailsView::Construct(const FArguments& InArgs)
{
	DetailsViewArgs = InArgs._DetailsViewArgs;
	bViewingClassDefaultObject = false;

	// Create the root property now
	RootPropertyNode = MakeShareable( new FObjectPropertyNode );
		
	PropertyUtilities = MakeShareable( new FPropertyDetailsUtilities( *this ) );
	
	ColumnSizeData.LeftColumnWidth = TAttribute<float>( this, &SDetailsView::OnGetLeftColumnWidth );
	ColumnSizeData.RightColumnWidth = TAttribute<float>( this, &SDetailsView::OnGetRightColumnWidth );
	ColumnSizeData.OnWidthChanged = SSplitter::FOnSlotResized::CreateSP( this, &SDetailsView::OnSetColumnWidth );

	TSharedRef<SScrollBar> ExternalScrollbar = 
		SNew(SScrollBar)
		.AlwaysShowScrollbar( true );

		FMenuBuilder DetailViewOptions( true, NULL );

		FUIAction ShowOnlyModifiedAction( 
			FExecuteAction::CreateSP( this, &SDetailsView::OnShowOnlyModifiedClicked ),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP( this, &SDetailsView::IsShowOnlyModifiedChecked )
		);

		if (DetailsViewArgs.bShowModifiedPropertiesOption)
		{
			DetailViewOptions.AddMenuEntry( 
				LOCTEXT("ShowOnlyModified", "Show Only Modified Properties"),
				LOCTEXT("ShowOnlyModified_ToolTip", "Displays only properties which have been changed from their default"),
				FSlateIcon(),
				ShowOnlyModifiedAction,
				NAME_None,
				EUserInterfaceActionType::ToggleButton 
			);
		}

		if( DetailsViewArgs.bShowDifferingPropertiesOption )
		{
			DetailViewOptions.AddMenuEntry(
				LOCTEXT("ShowOnlyDiffering", "Show Only Differing Properties"),
				LOCTEXT("ShowOnlyDiffering_ToolTip", "Displays only properties in this instance which have been changed or added from the instance being compared"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &SDetailsView::OnShowOnlyDifferingClicked),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(this, &SDetailsView::IsShowOnlyDifferingChecked)
				),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);
		}

		FUIAction ShowAllAdvancedAction( 
			FExecuteAction::CreateSP( this, &SDetailsView::OnShowAllAdvancedClicked ),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP( this, &SDetailsView::IsShowAllAdvancedChecked )
		);

		DetailViewOptions.AddMenuEntry(
			LOCTEXT("ShowAllAdvanced", "Show All Advanced Details"),
			LOCTEXT("ShowAllAdvanced_ToolTip", "Shows all advanced detail sections in each category"),
			FSlateIcon(),
			ShowAllAdvancedAction,
			NAME_None,
			EUserInterfaceActionType::ToggleButton 
			);

		DetailViewOptions.AddMenuEntry(
			LOCTEXT("CollapseAll", "Collapse All Categories"),
			LOCTEXT("CollapseAll_ToolTip", "Collapses all root level categories"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SDetailsView::SetRootExpansionStates, /*bExpanded=*/false, /*bRecurse=*/false )));
		DetailViewOptions.AddMenuEntry(
			LOCTEXT("ExpandAll", "Expand All Categories"),
			LOCTEXT("ExpandAll_ToolTip", "Expands all root level categories"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SDetailsView::SetRootExpansionStates, /*bExpanded=*/true, /*bRecurse=*/false )));

	TSharedRef<SHorizontalBox> FilterRow = SNew( SHorizontalBox )
		.Visibility( this, &SDetailsView::GetFilterBoxVisibility )
		+SHorizontalBox::Slot()
		.FillWidth( 1 )
		.VAlign( VAlign_Center )
		[
			// Create the search box
			SAssignNew( SearchBox, SSearchBox )
			.OnTextChanged( this, &SDetailsView::OnFilterTextChanged  )
			.AddMetaData<FTagMetaData>(TEXT("Details.Search"))
		]
		+SHorizontalBox::Slot()
		.Padding( 4.0f, 0.0f, 0.0f, 0.0f )
		.AutoWidth()
		[
			// Create the search box
			SNew( SButton )
			.OnClicked( this, &SDetailsView::OnOpenRawPropertyEditorClicked )
			.IsEnabled( this, &SDetailsView::IsPropertyEditingEnabled )
			.ToolTipText( LOCTEXT("RawPropertyEditorButtonLabel", "Open Selection in Property Matrix") )
			[
				SNew( SImage )
				.Image( FEditorStyle::GetBrush("DetailsView.EditRawProperties") )
			]
		];

	if (DetailsViewArgs.bShowOptions)
	{
		FilterRow->AddSlot()
			.HAlign(HAlign_Right)
			.AutoWidth()
			[
				SNew( SComboButton )
				.ContentPadding(0)
				.ForegroundColor( FSlateColor::UseForeground() )
				.ButtonStyle( FEditorStyle::Get(), "ToggleButton" )
				.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ViewOptions")))
				.MenuContent()
				[
					DetailViewOptions.MakeWidget()
				]
				.ButtonContent()
				[
					SNew(SImage)
					.Image( FEditorStyle::GetBrush("GenericViewButton") )
				]
			];
	}


	ChildSlot
	[
		SNew( SVerticalBox )
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding( 0.0f, 0.0f, 0.0f, 4.0f )
		[
			// Create the name area which does not change when selection changes
			SAssignNew( NameArea, SDetailNameArea, &SelectedObjects )
			// the name area is only for actors
			.Visibility( this, &SDetailsView::GetActorNameAreaVisibility  )
			.OnLockButtonClicked( this, &SDetailsView::OnLockButtonClicked )
			.IsLocked( this, &SDetailsView::IsLocked )
			.ShowLockButton( DetailsViewArgs.bLockable )
			.ShowActorLabel( DetailsViewArgs.bShowActorLabel )
			// only show the selection tip if we're not selecting objects
			.SelectionTip( !DetailsViewArgs.bHideSelectionTip )
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding( 0.0f, 0.0f, 0.0f, 2.0f )
		[
			FilterRow
		]
		+ SVerticalBox::Slot()
		.FillHeight(1)
		.Padding(0)
		[
			SNew( SHorizontalBox )
			+ SHorizontalBox::Slot()
			[
				ConstructTreeView( ExternalScrollbar )
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew( SBox )
				.WidthOverride( 16.0f )
				[
					ExternalScrollbar
				]
			]
		]
	];
}

TSharedRef<SDetailTree> SDetailsView::ConstructTreeView( TSharedRef<SScrollBar>& ScrollBar )
{
	check( !DetailTree.IsValid() || DetailTree.IsUnique() );

	return 
	SAssignNew( DetailTree, SDetailTree )
	.Visibility( this, &SDetailsView::GetTreeVisibility )
	.TreeItemsSource( &RootTreeNodes )
	.OnGetChildren( this, &SDetailsView::OnGetChildrenForDetailTree )
	.OnSetExpansionRecursive( this, &SDetailsView::SetNodeExpansionStateRecursive )
	.OnGenerateRow( this, &SDetailsView::OnGenerateRowForDetailTree )	
	.OnExpansionChanged( this, &SDetailsView::OnItemExpansionChanged )
	.SelectionMode( ESelectionMode::None )
	.ExternalScrollbar( ScrollBar );
}

FReply SDetailsView::OnOpenRawPropertyEditorClicked()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>( "PropertyEditor" );
	PropertyEditorModule.CreatePropertyEditorToolkit( EToolkitMode::Standalone, TSharedPtr<IToolkitHost>(), SelectedObjects );

	return FReply::Handled();
}

EVisibility SDetailsView::GetActorNameAreaVisibility() const
{
	const bool bVisible = !DetailsViewArgs.bHideActorNameArea && !bViewingClassDefaultObject;
	return bVisible ? EVisibility::Visible : EVisibility::Collapsed; 
}

void SDetailsView::ForceRefresh()
{
	TArray< TWeakObjectPtr< UObject > > NewObjectList;

	// Simply re-add the same existing objects to cause a refresh
	for ( TPropObjectIterator Itor( RootPropertyNode->ObjectIterator() ); Itor; ++Itor )
	{
		TWeakObjectPtr<UObject> Object = *Itor;
		if( Object.IsValid() )
		{
			NewObjectList.Add( Object.Get() );
		}
	}

	SetObjectArrayPrivate( NewObjectList );
}


void SDetailsView::SetObjects( const TArray<UObject*>& InObjects, bool bForceRefresh/* = false*/ )
{
	if( !IsLocked() )
	{
		TArray< TWeakObjectPtr< UObject > > ObjectWeakPtrs;
		
		for( auto ObjectIter = InObjects.CreateConstIterator(); ObjectIter; ++ObjectIter )
		{
			ObjectWeakPtrs.Add( *ObjectIter );
		}

 		if( bForceRefresh || ShouldSetNewObjects( ObjectWeakPtrs ) )
		{
			SetObjectArrayPrivate( ObjectWeakPtrs );
		}
	}
}

void SDetailsView::SetObjects( const TArray< TWeakObjectPtr< UObject > >& InObjects, bool bForceRefresh/* = false*/ )
{
	if( !IsLocked() )
	{
		if( bForceRefresh || ShouldSetNewObjects( InObjects ) )
		{
			SetObjectArrayPrivate( InObjects );
		}
	}
}

void SDetailsView::SetObject( UObject* InObject, bool bForceRefresh )
{
	TArray< TWeakObjectPtr< UObject > > ObjectWeakPtrs;
	ObjectWeakPtrs.Add( InObject );

	SetObjects( ObjectWeakPtrs, bForceRefresh );
}

bool SDetailsView::ShouldSetNewObjects( const TArray< TWeakObjectPtr< UObject > >& InObjects ) const
{
	bool bShouldSetObjects = false;
	if( InObjects.Num() != RootPropertyNode->GetNumObjects() )
	{
		// If the object arrys differ in size then at least one object is different so we must reset
		bShouldSetObjects = true;
	}
	else
	{
		// Check to see if the objects passed in are different. If not we do not need to set anything
		TSet< TWeakObjectPtr< UObject > > NewObjects;
		NewObjects.Append( InObjects );
		for ( TPropObjectIterator Itor( RootPropertyNode->ObjectIterator() ); Itor; ++Itor )
		{
			TWeakObjectPtr<UObject> Object = *Itor;
			if( Object.IsValid() && !NewObjects.Contains( Object ) )
			{
				// An existing object is not in the list of new objects to set
				bShouldSetObjects = true;
				break;
			}
			else if( !Object.IsValid() )
			{
				// An existing object is invalid
				bShouldSetObjects = true;
				break;
			}
		}
	}

	return bShouldSetObjects;
}

void SDetailsView::SetObjectArrayPrivate( const TArray< TWeakObjectPtr< UObject > >& InObjects )
{
	double StartTime = FPlatformTime::Seconds();

	PreSetObject();

	check( RootPropertyNode.IsValid() );

	// Selected actors for building SelectedActorInfo
	TArray<AActor*> SelectedRawActors;

	bViewingClassDefaultObject = false;
	bool bOwnedByLockedLevel = false;
	for( int32 ObjectIndex = 0 ; ObjectIndex < InObjects.Num() ; ++ObjectIndex )
	{
		TWeakObjectPtr< UObject > Object = InObjects[ObjectIndex];

		if( Object.IsValid() )
		{
			bViewingClassDefaultObject |= Object->HasAnyFlags( RF_ClassDefaultObject );

			RootPropertyNode->AddObject( Object.Get() );
			SelectedObjects.Add( Object );
			AActor* Actor = Cast<AActor>( Object.Get() );
			if( Actor )
			{
				SelectedActors.Add( Actor );
				SelectedRawActors.Add( Actor );
			}
		}
	}

	if( InObjects.Num() == 0 )
	{
		// Unlock the view automatically if we are viewing nothing
		bIsLocked = false;
	}

	// Selection changed, refresh the detail area
	if ( DetailsViewArgs.bObjectsUseNameArea )
	{
		NameArea->Refresh( SelectedObjects );
	}
	else
	{
		NameArea->Refresh( SelectedActors );
	}
	
	// When selection changes rebuild information about the selection
	SelectedActorInfo = AssetSelectionUtils::BuildSelectedActorInfo( SelectedRawActors );

	// @todo Slate Property Window
	//SetFlags(EPropertyWindowFlags::ReadOnly, bOwnedByLockedLevel);


	PostSetObject();

	// Set the title of the window based on the objects we are viewing
	// Or call the delegate for handling when the title changed
	FString Title;

	if( !RootPropertyNode->GetObjectBaseClass() )
	{
		Title = NSLOCTEXT("PropertyView", "NothingSelectedTitle", "Nothing selected").ToString();
	}
	else if( RootPropertyNode->GetNumObjects() == 1 )
	{
		// if the object is the default metaobject for a UClass, use the UClass's name instead
		const UObject* Object = RootPropertyNode->ObjectConstIterator()->Get();
		FString ObjectName = Object->GetName();
		if ( Object->GetClass()->GetDefaultObject() == Object )
		{
			ObjectName = Object->GetClass()->GetName();
		}
		else
		{
			// Is this an actor?  If so, it might have a friendly name to display
			const AActor* Actor = Cast<const  AActor >( Object );
			if( Actor != NULL )
			{
				// Use the friendly label for this actor
				ObjectName = Actor->GetActorLabel();
			}
		}

		Title = ObjectName;
	}
	else
	{
		Title = FString::Printf( *NSLOCTEXT("PropertyView", "MultipleSelected", "%s (%i selected)").ToString(), *RootPropertyNode->GetObjectBaseClass()->GetName(), RootPropertyNode->GetNumObjects() );
	}

	OnObjectArrayChanged.ExecuteIfBound(Title, InObjects);

	double ElapsedTime = FPlatformTime::Seconds() - StartTime;
}

void SDetailsView::ReplaceObjects( const TMap<UObject*, UObject*>& OldToNewObjectMap )
{
	TArray< TWeakObjectPtr< UObject > > NewObjectList;
	bool bObjectsReplaced = false;

	TArray< FObjectPropertyNode* > ObjectNodes;
	PropertyEditorHelpers::CollectObjectNodes( RootPropertyNode, ObjectNodes );

	for( int32 ObjectNodeIndex = 0; ObjectNodeIndex < ObjectNodes.Num(); ++ObjectNodeIndex )
	{
		FObjectPropertyNode* CurrentNode = ObjectNodes[ObjectNodeIndex];

		// Scan all objects and look for objects which need to be replaced
		for ( TPropObjectIterator Itor( CurrentNode->ObjectIterator() ); Itor; ++Itor )
		{
			UObject* Replacement = OldToNewObjectMap.FindRef( Itor->Get() );
			if( Replacement && Replacement->GetClass() == Itor->Get()->GetClass() )
			{
				bObjectsReplaced = true;
				if( CurrentNode == RootPropertyNode.Get() )
				{
					// Note: only root objects count for the new object list. Sub-Objects (i.e components count as needing to be replaced but they don't belong in the top level object list
					NewObjectList.Add( Replacement );
				}
			}
			else if( CurrentNode == RootPropertyNode.Get() )
			{
				// Note: only root objects count for the new object list. Sub-Objects (i.e components count as needing to be replaced but they don't belong in the top level object list
				NewObjectList.Add( Itor->Get() );
			}
		}
	}


	if( bObjectsReplaced )
	{
		SetObjectArrayPrivate( NewObjectList );
	}

}

void SDetailsView::RemoveDeletedObjects( const TArray<UObject*>& DeletedObjects )
{
	TArray< TWeakObjectPtr< UObject > > NewObjectList;
	bool bObjectsRemoved = false;

	// Scan all objects and look for objects which need to be replaced
	for ( TPropObjectIterator Itor( RootPropertyNode->ObjectIterator() ); Itor; ++Itor )
	{
		if( DeletedObjects.Contains( Itor->Get() ) )
		{
			// An object we had needs to be removed
			bObjectsRemoved = true;
		}
		else
		{
			// If the deleted object list does not contain the current object, its ok to keep it in the list
			NewObjectList.Add( Itor->Get() );
		}
	}

	// if any objects were replaced update the observed objects
	if( bObjectsRemoved )
	{
		SetObjectArrayPrivate( NewObjectList );
	}
}

/**
 * Removes actors from the property nodes object array which are no longer available
 * 
 * @param ValidActors	The list of actors which are still valid
 */
void SDetailsView::RemoveInvalidActors( const TSet<AActor*>& ValidActors )
{
	TArray< TWeakObjectPtr< UObject > > ResetArray;

	bool bAllFound = true;
	for ( TPropObjectIterator Itor( RootPropertyNode->ObjectIterator() ); Itor; ++Itor )
	{
		AActor* Actor = Cast<AActor>( Itor->Get() );

		bool bFound = ValidActors.Contains( Actor );

		// If the selected actor no longer exists, remove it from the property window.
		if( bFound )
		{
			ResetArray.Add(Actor);
		}
		else
		{
			bAllFound = false;
		}
	}

	if ( !bAllFound ) 
	{
		SetObjectArrayPrivate( ResetArray );
	}
}

/** Called before during SetObjectArray before we change the objects being observed */
void SDetailsView::PreSetObject()
{
	ExternalRootPropertyNodes.Empty();

	// Save existing expanded items first
	SaveExpandedItems();

	RootNodePendingKill = RootPropertyNode;

	RootPropertyNode = MakeShareable(new FObjectPropertyNode);

	SelectedActors.Empty();
	SelectedObjects.Empty();
}


/** Called at the end of SetObjectArray after we change the objects being observed */
void SDetailsView::PostSetObject()
{
	DestroyColorPicker();
	ColorPropertyNode = NULL;

	FPropertyNodeInitParams InitParams;
	InitParams.ParentNode = NULL;
	InitParams.Property = NULL;
	InitParams.ArrayOffset = 0;
	InitParams.ArrayIndex = INDEX_NONE;
	InitParams.bAllowChildren = true;
	InitParams.bForceHiddenPropertyVisibility =  FPropertySettings::Get().ShowHiddenProperties();

	RootPropertyNode->InitNode( InitParams );

	bool bInitiallySeen = true;
	bool bParentAllowsVisible = true;
	// Restore existing expanded items
	RestoreExpandedItems();

	UpdatePropertyMap();
}

void SDetailsView::SetOnObjectArrayChanged(FOnObjectArrayChanged OnObjectArrayChangedDelegate)
{
	OnObjectArrayChanged = OnObjectArrayChangedDelegate;
}

const UClass* SDetailsView::GetBaseClass() const
{
	if( RootPropertyNode.IsValid() )
	{
		return RootPropertyNode->GetObjectBaseClass();
	}

	return NULL;
}

UClass* SDetailsView::GetBaseClass()
{
	if( RootPropertyNode.IsValid() )
	{
		return RootPropertyNode->GetObjectBaseClass();
	}

	return NULL;
}

UStruct* SDetailsView::GetBaseStruct() const
{
	return RootPropertyNode.IsValid() ? RootPropertyNode->GetObjectBaseClass() : NULL;
}

void SDetailsView::RegisterInstancedCustomPropertyLayout( UClass* Class, FOnGetDetailCustomizationInstance DetailLayoutDelegate )
{
	check( Class );

	FDetailLayoutCallback Callback;
	Callback.DetailLayoutDelegate = DetailLayoutDelegate;
	// @todo: DetailsView: Fix me: this specifies the order in which detail layouts should be queried
	Callback.Order = InstancedClassToDetailLayoutMap.Num();

	InstancedClassToDetailLayoutMap.Add( Class, Callback );
}

void SDetailsView::UnregisterInstancedCustomPropertyLayout( UClass* Class )
{
	check( Class );

	InstancedClassToDetailLayoutMap.Remove( Class );
}

void SDetailsView::AddExternalRootPropertyNode( TSharedRef<FObjectPropertyNode> ExternalRootNode )
{
	ExternalRootPropertyNodes.Add( ExternalRootNode );
}

bool SDetailsView::IsCategoryHiddenByClass( FName CategoryName ) const
{
	return RootPropertyNode->GetHiddenCategories().Contains( CategoryName );
}

bool SDetailsView::IsConnected() const
{
	return RootPropertyNode.IsValid() && (RootPropertyNode->GetNumObjects() > 0);
}

#undef LOCTEXT_NAMESPACE
