// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "FriendViewModel.h"
#include "FriendsViewModel.h"
#include "FriendListViewModel.h"

class FFriendListViewModelImpl
	: public FFriendListViewModel
{
public:

	~FFriendListViewModelImpl()
	{
		Uninitialize();
	}

	virtual const TArray< TSharedPtr< class FFriendViewModel > >& GetFriendsList() const override
	{
		return FriendsList;
	}

	virtual FText GetListCountText() const override
	{
		return FText::AsNumber(FriendsList.Num());
	}

	virtual const FText GetListName() const override
	{
		return EFriendsDisplayLists::ToFText(ListType);
	}

	virtual const EFriendsDisplayLists::Type GetListType() const override
	{
		return ListType;
	}

	DECLARE_DERIVED_EVENT(FFriendListViewModelImpl , FFriendListViewModel::FFriendsListUpdated, FFriendsListUpdated);
	virtual FFriendsListUpdated& OnFriendsListUpdated() override
	{
		return FriendsListUpdatedEvent;
	}

private:
	void Initialize()
	{
		FFriendsAndChatManager::Get()->OnFriendsListUpdated().AddSP( this, &FFriendListViewModelImpl::RefreshFriendsList );
		RefreshFriendsList();
	}

	void Uninitialize()
	{
	}

	void RefreshFriendsList()
	{
		FriendsList.Empty();

		TArray< TSharedPtr< FFriendStuct > > FriendItemList;
		FFriendsAndChatManager::Get()->GetFilteredFriendsList( FriendItemList );
		for( const auto& FriendItem : FriendItemList)
		{
			switch (ListType)
			{
				case EFriendsDisplayLists::DefaultDisplay :
				{
					if(FriendItem->GetInviteStatus() == EInviteStatus::Accepted)
					{
						FriendsList.Add(FFriendViewModelFactory::Create(FriendItem.ToSharedRef()));
					}
				}
				break;
				case EFriendsDisplayLists::RecentPlayersDisplay :
				{
				}
				break;
				case EFriendsDisplayLists::FriendRequestsDisplay :
				{
					if( FriendItem->GetInviteStatus() == EInviteStatus::PendingInbound)
					{
						FriendsList.Add(FFriendViewModelFactory::Create(FriendItem.ToSharedRef()));
					}
				}
				break;
			}
		}
		FriendsListUpdatedEvent.Broadcast();
	}

	FFriendListViewModelImpl(
		const TSharedRef<FFriendsViewModel>& FriendsViewModel,
		EFriendsDisplayLists::Type ListType
		)
		: ViewModel(FriendsViewModel)
		, ListType(ListType)
	{
	}

private:
	const TSharedRef<FFriendsViewModel> ViewModel;
	const EFriendsDisplayLists::Type ListType;

	/** Holds the list of friends. */
	TArray< TSharedPtr< FFriendViewModel > > FriendsList;
	FFriendsListUpdated FriendsListUpdatedEvent;

	friend FFriendListViewModelFactory;
};

TSharedRef< FFriendListViewModel > FFriendListViewModelFactory::Create(
	const TSharedRef<FFriendsViewModel>& FriendsViewModel,
	EFriendsDisplayLists::Type ListType
	)
{
	TSharedRef< FFriendListViewModelImpl > ViewModel(new FFriendListViewModelImpl(FriendsViewModel, ListType));
	ViewModel->Initialize();
	return ViewModel;
}