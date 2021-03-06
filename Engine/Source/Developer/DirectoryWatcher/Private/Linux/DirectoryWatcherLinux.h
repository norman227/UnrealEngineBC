// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

class FDirectoryWatcherLinux : public IDirectoryWatcher
{
public:
	FDirectoryWatcherLinux();
	virtual ~FDirectoryWatcherLinux();

	virtual bool RegisterDirectoryChangedCallback(const FString& Directory, const FDirectoryChanged& InDelegate) override;
	virtual bool UnregisterDirectoryChangedCallback(const FString& Directory, const FDirectoryChanged& InDelegate) override;
	virtual void Tick (float DeltaSeconds) override;

	/** Map of directory paths to requests */
	TMap<FString, FDirectoryWatchRequestLinux*> RequestMap;
	TArray<FDirectoryWatchRequestLinux*> RequestsPendingDelete;

	/** A count of FDirectoryWatchRequestLinux created to ensure they are cleaned up on shutdown */
	int32 NumRequests;
};

typedef FDirectoryWatcherLinux FDirectoryWatcher;
