// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/inotify.h>

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

class FDirectoryWatchRequestLinux
{
public:
	FDirectoryWatchRequestLinux();
	virtual ~FDirectoryWatchRequestLinux();

	/** Sets up the directory handle and request information */
	bool Init(const FString& InDirectory);

	/** Adds a delegate to get fired when the directory changes */
	void AddDelegate( const IDirectoryWatcher::FDirectoryChanged& InDelegate );
	/** Removes a delegate to get fired when the directory changes */
	bool RemoveDelegate( const IDirectoryWatcher::FDirectoryChanged& InDelegate );
	/** Returns true if this request has any delegates listening to directory changes */
	bool HasDelegates() const;
	/** Prepares the request for deletion */
	void EndWatchRequest();
	/** Triggers all pending file change notifications */
	void ProcessPendingNotifications();

private:

	FString Directory;

	bool bRunning;
	bool bEndWatchRequestInvoked;

	int FileDescriptor;
	int * WatchDescriptor;
	int NotifyFilter;

	TArray<FString> AllFiles;
	TArray<IDirectoryWatcher::FDirectoryChanged> Delegates;
	TArray<FFileChangeData> FileChanges;

	void Shutdown();
	void ProcessChanges();
};
