// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "LayersPrivatePCH.h"
#include "SLayersViewRow.h"
#include "SInlineEditableTextBlock.h"

#define LOCTEXT_NAMESPACE "LayersView"

void SLayersViewRow::Construct(const FArguments& InArgs, TSharedRef< FLayerViewModel > InViewModel, TSharedRef< STableViewBase > InOwnerTableView)
{
	ViewModel = InViewModel;

	HighlightText = InArgs._HighlightText;

	SMultiColumnTableRow< TSharedPtr< FLayerViewModel > >::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}

SLayersViewRow::~SLayersViewRow()
{
	ViewModel->OnRenamedRequest().RemoveSP(InlineTextBlock.Get(), &SInlineEditableTextBlock::EnterEditingMode);
}

TSharedRef< SWidget > SLayersViewRow::GenerateWidgetForColumn(const FName& ColumnID)
{
	TSharedPtr< SWidget > TableRowContent;

	if (ColumnID == LayersView::ColumnID_LayerLabel)
	{
		TableRowContent =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 1.0f, 3.0f, 1.0f)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush(TEXT("Layer.Icon16x")))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]

		+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SAssignNew(InlineTextBlock, SInlineEditableTextBlock)
				.Font(FEditorStyle::GetFontStyle("LayersView.LayerNameFont"))
				.Text(ViewModel.Get(), &FLayerViewModel::GetNameAsText)
				.ColorAndOpacity(this, &SLayersViewRow::GetColorAndOpacity)
				.HighlightText(HighlightText)
				.ToolTipText(LOCTEXT("DoubleClickToolTip", "Double Click to Select All Actors"))
				.OnVerifyTextChanged(this, &SLayersViewRow::OnRenameLayerTextChanged)
				.OnTextCommitted(this, &SLayersViewRow::OnRenameLayerTextCommitted)
				.IsSelected(this, &SLayersViewRow::IsSelectedExclusively)
			]
		;

		ViewModel->OnRenamedRequest().AddSP(InlineTextBlock.Get(), &SInlineEditableTextBlock::EnterEditingMode);
	}
	else if (ColumnID == LayersView::ColumnID_Visibility)
	{
		TableRowContent =
			SAssignNew(VisibilityButton, SButton)
			.ContentPadding(0)
			.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
			.OnClicked(this, &SLayersViewRow::OnToggleVisibility)
			.ToolTipText(LOCTEXT("VisibilityButtonToolTip", "Toggle Layer Visibility"))
			.ForegroundColor(FSlateColor::UseForeground())
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Content()
			[
				SNew(SImage)
				.Image(this, &SLayersViewRow::GetVisibilityBrushForLayer)
				.ColorAndOpacity(this, &SLayersViewRow::GetForegroundColorForButton)
			]
		;
	}
	else
	{
		checkf(false, TEXT("Unknown ColumnID provided to SLayersView"));
	}

	return TableRowContent.ToSharedRef();
}

void SLayersViewRow::OnRenameLayerTextCommitted(const FText& InText, ETextCommit::Type eInCommitType)
{
	if (!InText.IsEmpty())
	{
		ViewModel->RenameTo(*InText.ToString());
	}
}

bool SLayersViewRow::OnRenameLayerTextChanged(const FText& NewText, FText& OutErrorMessage)
{
	FString OutMessage;
	if (!ViewModel->CanRenameTo(*NewText.ToString(), OutMessage))
	{
		OutErrorMessage = FText::FromString(OutMessage);
		return false;
	}

	return true;
}

void SLayersViewRow::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	TSharedPtr< FActorDragDropGraphEdOp > DragActorOp = DragDropEvent.GetOperationAs< FActorDragDropGraphEdOp >();
	if (DragActorOp.IsValid())
	{
		DragActorOp->ResetToDefaultToolTip();
	}
}

FReply SLayersViewRow::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr< FActorDragDropGraphEdOp > DragActorOp = DragDropEvent.GetOperationAs< FActorDragDropGraphEdOp >();
	if (!DragActorOp.IsValid())
	{
		return FReply::Unhandled();
	}

	bool bCanAssign = false;
	FText Message;
	if (DragActorOp->Actors.Num() > 1)
	{
		bCanAssign = ViewModel->CanAssignActors(DragActorOp->Actors, OUT Message);
	}
	else
	{
		bCanAssign = ViewModel->CanAssignActor(DragActorOp->Actors[0], OUT Message);
	}

	if (bCanAssign)
	{
		DragActorOp->SetToolTip(FActorDragDropGraphEdOp::ToolTip_CompatibleGeneric, Message);
	}
	else
	{
		DragActorOp->SetToolTip(FActorDragDropGraphEdOp::ToolTip_IncompatibleGeneric, Message);
	}

	return FReply::Handled();
}

FReply SLayersViewRow::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr< FActorDragDropGraphEdOp > DragActorOp = DragDropEvent.GetOperationAs< FActorDragDropGraphEdOp >();
	if (!DragActorOp.IsValid())
	{
		return FReply::Unhandled();
	}

	ViewModel->AddActors(DragActorOp->Actors);

	return FReply::Handled();
}

FSlateColor SLayersViewRow::GetForegroundColorForButton() const
{
	return (VisibilityButton.IsValid() && (VisibilityButton->IsHovered() || VisibilityButton->IsPressed())) ? FEditorStyle::GetSlateColor("InvertedForeground") : FSlateColor::UseForeground();
}

FSlateColor SLayersViewRow::GetColorAndOpacity() const
{
	if (!FSlateApplication::Get().IsDragDropping())
	{
		return FSlateColor::UseForeground();
	}

	bool bCanAcceptDrop = false;
	TSharedPtr<FDragDropOperation> DragDropOp = FSlateApplication::Get().GetDragDroppingContent();

	if (DragDropOp.IsValid() && DragDropOp->IsOfType<FActorDragDropGraphEdOp>())
	{
		TSharedPtr<FActorDragDropGraphEdOp> DragDropActorOp = StaticCastSharedPtr<FActorDragDropGraphEdOp>(DragDropOp);

		FText Message;
		bCanAcceptDrop = ViewModel->CanAssignActors(DragDropActorOp->Actors, OUT Message);
	}

	return (bCanAcceptDrop) ? FSlateColor::UseForeground() : FLinearColor(0.30f, 0.30f, 0.30f);
}

const FSlateBrush* SLayersViewRow::GetVisibilityBrushForLayer() const
{
	return (ViewModel->IsVisible()) ? FEditorStyle::GetBrush("Layer.VisibleIcon16x") : FEditorStyle::GetBrush("Layer.NotVisibleIcon16x");
}

#undef LOCTEXT_NAMESPACE
