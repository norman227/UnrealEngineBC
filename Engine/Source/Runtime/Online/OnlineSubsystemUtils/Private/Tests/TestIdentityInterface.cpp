// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemUtilsPrivatePCH.h"
#include "ModuleManager.h"
#include "TestIdentityInterface.h"

void FTestIdentityInterface::Test(UWorld* InWorld, const FOnlineAccountCredentials& InAccountCredentials, bool bOnlyRunLogoutTest)
{
	// Toggle the various tests to run
	if (bOnlyRunLogoutTest)
	{
		bRunLoginTest = false;
		bRunLogoutTest = true;
	}

	AccountCredentials = InAccountCredentials;

	OnlineIdentity = Online::GetIdentityInterface(InWorld, SubsystemName.Len() ? FName(*SubsystemName, FNAME_Find) : NAME_None);
	if (OnlineIdentity.IsValid())
	{
		// Add delegates for the various async calls
		OnlineIdentity->AddOnLoginCompleteDelegate(LocalUserIdx, OnLoginCompleteDelegate);
		OnlineIdentity->AddOnLogoutCompleteDelegate(LocalUserIdx, OnLogoutCompleteDelegate);
		
		// kick off next test
		StartNextTest();
	}
	else
	{
		UE_LOG(LogOnline, Warning,
			TEXT("Failed to get online identity interface for %s"), *SubsystemName);

		// done with the test
		FinishTest();
	}
}

void FTestIdentityInterface::StartNextTest()
{
	if (bRunLoginTest)
	{
		// no external account credentials so use internal secret key for login
		OnlineIdentity->Login(LocalUserIdx, AccountCredentials);
	}
	else if (bRunLogoutTest)
	{
		OnlineIdentity->Logout(LocalUserIdx);
	}
	else
	{
		// done with the test
		FinishTest();
	}
}

void FTestIdentityInterface::FinishTest()
{
	if (OnlineIdentity.IsValid())
	{
		// Clear delegates for the various async calls
		OnlineIdentity->ClearOnLoginCompleteDelegate(LocalUserIdx, OnLoginCompleteDelegate);
		OnlineIdentity->ClearOnLogoutCompleteDelegate(LocalUserIdx, OnLogoutCompleteDelegate);
	}
	delete this;
}

void FTestIdentityInterface::OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
	if (bWasSuccessful)
	{
		UE_LOG(LogOnline, Log, TEXT("Successful authenticated user. UserId=[%s] "), 
			*UserId.ToDebugString());

		// update user info for newly registered user
		UserInfo = OnlineIdentity->GetUserAccount(UserId);
	}
	else
	{
		UE_LOG(LogOnline, Warning, TEXT("Failed to authenticate new user. Error=[%s]"), 
			*Error);
	}
	// Mark test as done
	bRunLoginTest = false;
	// kick off next test
	StartNextTest();
}

void FTestIdentityInterface::OnLogoutComplete(int32 LocalUserNum, bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		UE_LOG(LogOnline, Log, TEXT("Successful logged out user. LocalUserNum=[%d] "),
			LocalUserNum);
	}
	else
	{
		UE_LOG(LogOnline, Warning, TEXT("Failed to log out user."));
	}
	UserInfo.Reset();
	// Mark test as done
	bRunLogoutTest = false;
	// kick off next test
	StartNextTest();
}

