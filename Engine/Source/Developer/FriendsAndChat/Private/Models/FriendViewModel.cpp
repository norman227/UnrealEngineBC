// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "FriendViewModel.h"

class FFriendViewModelImpl
	: public FFriendViewModel
{
public:

	virtual void EnumerateActions(TArray<EFriendActionType::Type>& Actions)
	{
		if(FriendItem->IsPendingAccepted())
		{
			Actions.Add(EFriendActionType::Updating);
		}
		else
		{
			switch (FriendItem->GetInviteStatus())
			{
				case EInviteStatus::Accepted :
				{
					Actions.Add(EFriendActionType::RemoveFriend);
					Actions.Add(EFriendActionType::BlockFriend);
					if( FFriendsAndChatManager::Get()->IsInSession())
					{
						Actions.Add(EFriendActionType::InviteToGame);
					}
					if(FriendItem->IsOnline())
					{
						Actions.Add(EFriendActionType::JoinGame);
					}
				}
				break;
				case EInviteStatus::PendingInbound :
				{
					Actions.Add(EFriendActionType::AcceptFriendRequest);
					Actions.Add(EFriendActionType::IgnoreFriendRequest);
					Actions.Add(EFriendActionType::BlockFriend);
				}
				break;
				case EInviteStatus::PendingOutbound :
				{
					// Do Nothing
				}
				break;
			default:
				Actions.Add(EFriendActionType::SendFriendRequest);
				break;
			}
		}
	}

	virtual void PerformAction(const EFriendActionType::Type ActionType)
	{
		switch(ActionType)
		{
			case EFriendActionType::AcceptFriendRequest : AcceptFriend(); break;
			case EFriendActionType::RemoveFriend :
			case EFriendActionType::IgnoreFriendRequest :
			case EFriendActionType::BlockFriend :
			{
				RemoveFriend();
			}
			break;
			case EFriendActionType::SendFriendRequest : SendFriendRequest(); break;
			case EFriendActionType::InviteToGame : InviteToGame(); break;
			case EFriendActionType::JoinGame : JoinGame(); break;
		}
	}

	~FFriendViewModelImpl()
	{
		Uninitialize();
	}

	virtual FText GetFriendName() const override
	{
		return FText::FromString(FriendItem->GetName());
	}

	virtual FText GetFriendLocation() const override
	{
		return FriendItem.IsValid() ? FriendItem->GetFriendLocation() : FText::GetEmpty();
	}

	virtual bool IsOnline() const override
	{
		return FriendItem.IsValid() ? FriendItem->IsOnline() : false;
	}

	void RemoveFriend() const
	{
		FFriendsAndChatManager::Get()->DeleteFriend( FriendItem );
	}

	void AcceptFriend() const
	{
		FFriendsAndChatManager::Get()->AcceptFriend( FriendItem );
	}

	void SendFriendRequest() const
	{
		if ( FriendItem.IsValid() && FriendItem->GetOnlineFriend().IsValid() )
		{
			FFriendsAndChatManager::Get()->RequestFriend(FText::FromString(FriendItem->GetName()));
		}
	}

	void InviteToGame()
	{
		if ( FriendItem.IsValid() && FriendItem->GetOnlineFriend().IsValid() )
		{
			FFriendsMessageManager::Get()->InviteFriendToGame( FriendItem->GetOnlineFriend()->GetUserId() );
		}
	}

	void JoinGame()
	{
		if ( FriendItem.IsValid() && FriendItem->GetOnlineFriend().IsValid() )
		{
			FFriendsMessageManager::Get()->RequestJoinAGame( FriendItem->GetOnlineFriend()->GetUserId() );
		}
	}

private:
	void Initialize()
	{
	}

	void Uninitialize()
	{
	}

	FFriendViewModelImpl(
		const TSharedRef<FFriendStuct>& FriendItem
		)
		: FriendItem(FriendItem)
	{
	}

private:
	TSharedPtr<FFriendStuct> FriendItem;

private:
	friend FFriendViewModelFactory;
};

TSharedRef< FFriendViewModel > FFriendViewModelFactory::Create(
	const TSharedRef<FFriendStuct>& FriendItem
	)
{
	TSharedRef< FFriendViewModelImpl > ViewModel(new FFriendViewModelImpl(FriendItem));
	ViewModel->Initialize();
	return ViewModel;
}