// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

class SFriendsList : public SUserWidget
{
public:

	SLATE_USER_ARGS(SFriendsList)
	{ }
	SLATE_ARGUMENT(const FFriendsAndChatStyle*, FriendStyle)
	SLATE_ARGUMENT( SMenuAnchor::EMethod, Method )
	SLATE_END_ARGS()

	/**
	 * Constructs the Friends list widget.
	 *
	 * @param InArgs The Slate argument list.
	 * @param ViewModel The widget view model.
	 */
	virtual void Construct(const FArguments& InArgs, const TSharedRef<class FFriendListViewModel>& ViewModel) = 0;
};
