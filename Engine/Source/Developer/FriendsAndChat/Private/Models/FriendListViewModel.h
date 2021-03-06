// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

class FFriendListViewModel
	: public TSharedFromThis<FFriendListViewModel>
{
public:
	virtual ~FFriendListViewModel() {}
	virtual const TArray< TSharedPtr< class FFriendViewModel > >& GetFriendsList() const = 0;
	virtual FText GetListCountText() const = 0;
	virtual const FText GetListName() const = 0;
	virtual const EFriendsDisplayLists::Type GetListType() const = 0;

	DECLARE_EVENT(FFriendListViewModel, FFriendsListUpdated)
	virtual FFriendsListUpdated& OnFriendsListUpdated() = 0;
};

/**
 * Creates the implementation for an FriendListViewModel.
 *
 * @return the newly created FFriendListViewModel implementation.
 */
FACTORY(TSharedRef< FFriendListViewModel >, FFriendListViewModel,
	const TSharedRef<class FFriendsViewModel>& FriendsViewModel, EFriendsDisplayLists::Type ListType);