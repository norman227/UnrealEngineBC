// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

// Enum holding the state of the Friends manager
namespace EFriendActionType
{
	enum Type
	{
		AcceptFriendRequest,		// Accept an incoming Friend request
		IgnoreFriendRequest,		// Ignore an incoming Friend request
		RejectFriendRequest,		// Reject an incoming Friend request
		BlockFriend,				// Block a friend
		RemoveFriend,				// Remove a friend
		JoinGame,					// Join a Friends game
		InviteToGame,				// Invite to game
		SendFriendRequest,			// Send a friend request
		Updating,					// Updating;
	};

	inline FText ToText(EFriendActionType::Type State)
	{
		switch (State)
		{
			case AcceptFriendRequest: return NSLOCTEXT("FriendsList","AcceptFriendRequest", "Accept");
			case IgnoreFriendRequest: return NSLOCTEXT("FriendsList","IgnoreFriendRequest", "Ignore");
			case RejectFriendRequest: return NSLOCTEXT("FriendsList","RejectFriendRequest", "Reject");
			case BlockFriend: return NSLOCTEXT("FriendsList","BlockFriend", "Block");
			case RemoveFriend: return NSLOCTEXT("FriendsList","RemoveFriend", "Remove");
			case JoinGame: return NSLOCTEXT("FriendsList","JoingGame", "Join Game");
			case InviteToGame: return NSLOCTEXT("FriendsList","Invite to game", "Invite to Game");
			case SendFriendRequest: return NSLOCTEXT("FriendsList","SendFriendRequst", "Send Friend Request");
			case Updating: return NSLOCTEXT("FriendsList","Updating", "Updating");

			default: return FText::GetEmpty();
		}
	}
};

class FFriendViewModel
	: public TSharedFromThis<FFriendViewModel>
{
public:
	virtual ~FFriendViewModel() {}
	virtual void EnumerateActions(TArray<EFriendActionType::Type>& Actions) = 0;
	virtual void PerformAction(const EFriendActionType::Type ActionType) = 0;
	virtual FText GetFriendName() const = 0;
	virtual FText GetFriendLocation() const = 0;
	virtual bool IsOnline() const = 0;
};

/**
 * Creates the implementation for an FriendViewModel.
 *
 * @return the newly created FriendViewModel implementation.
 */
FACTORY(TSharedRef< FFriendViewModel >, FFriendViewModel,
	const TSharedRef<class FFriendStuct>& FFriendStuct);