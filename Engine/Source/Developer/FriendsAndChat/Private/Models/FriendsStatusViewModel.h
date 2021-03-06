// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

class FFriendsStatusViewModel
	: public TSharedFromThis<FFriendsStatusViewModel>
{
public:
	virtual ~FFriendsStatusViewModel() {}
	virtual bool GetOnlineStatus() const = 0;
	virtual void SetOnlineStatus( const bool bOnlineStatus) = 0;
	virtual TArray<TSharedRef<FText> > GetStatusList() const = 0;
	virtual FText GetStatusText() const = 0;
};

/**
 * Creates the implementation for an FFriendsStatusViewModel.
 *
 * @return the newly created FFriendsStatusViewModel implementation.
 */
FACTORY(TSharedRef< FFriendsStatusViewModel >, FFriendsStatusViewModel,
	const TSharedRef<class FFriendsAndChatManager>& FFriendsAndChatManager );