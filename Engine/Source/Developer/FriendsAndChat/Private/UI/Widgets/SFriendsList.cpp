// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "SFriendsList.h"
#include "FriendsViewModel.h"
#include "FriendListViewModel.h"

#define LOCTEXT_NAMESPACE "SFriendsList"

/**
 * Declares the Friends List display widget
*/
class SFriendsListImpl : public SFriendsList
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FFriendListViewModel>& ViewModel)
	{
		FriendStyle = *InArgs._FriendStyle;
		this->ViewModel = ViewModel;
		ViewModel->OnFriendsListUpdated().AddSP(this, &SFriendsListImpl::RefreshFriendsList);
		MenuMethod = InArgs._Method;

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SAssignNew(Contents, SBorder)
		]);

		RefreshFriendsList();
	}

private:
	void RefreshFriendsList()
	{
		Contents->ClearContent();

		TSharedPtr<SVerticalBox> FriendsContents;
		SAssignNew(FriendsContents, SVerticalBox);

		for(const auto& OnlineFriend : ViewModel->GetFriendsList())
		{
			FriendsContents->AddSlot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			[
				SNew(SFriendItem, OnlineFriend.ToSharedRef())
				.FriendStyle(&FriendStyle)
				.Method(MenuMethod)
			];
		}

		Contents->SetContent(FriendsContents.ToSharedRef());
	}

private:
	/** Holds the style to use when making the widget. */
	FFriendsAndChatStyle FriendStyle;
	TSharedPtr<FFriendListViewModel> ViewModel;
	TSharedPtr<SBorder> Contents;
	SMenuAnchor::EMethod MenuMethod;
};

TSharedRef<SFriendsList> SFriendsList::New()
{
	return MakeShareable(new SFriendsListImpl());
}

#undef LOCTEXT_NAMESPACE
