// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemGooglePlayPrivatePCH.h"
#include "OnlineAsyncTaskGooglePlayQueryInAppPurchases.h"

FOnlineAsyncTaskGooglePlayQueryInAppPurchases::FOnlineAsyncTaskGooglePlayQueryInAppPurchases(
	FOnlineSubsystemGooglePlay* InSubsystem,
	const TArray<FString> InProductIds,
	const TArray<bool> InIsConsumableFlags)
	: FOnlineAsyncTaskBasic(InSubsystem)
	, ProductIds(InProductIds)
	, IsConsumableFlags(InIsConsumableFlags)

{
}


void FOnlineAsyncTaskGooglePlayQueryInAppPurchases::ProcessQueryAvailablePurchasesResults(bool bInSuccessful)
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineAsyncTaskGooglePlayQueryInAppPurchases::ProcessQueryAvailablePurchasesResults"));

	bWasSuccessful = bInSuccessful;
	bIsComplete = true;
}


void FOnlineAsyncTaskGooglePlayQueryInAppPurchases::Finalize()
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineAsyncTaskGooglePlayQueryInAppPurchases::Finalize"));
}


void FOnlineAsyncTaskGooglePlayQueryInAppPurchases::TriggerDelegates()
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineAsyncTaskGooglePlayQueryInAppPurchases::TriggerDelegates"));
	Subsystem->GetStoreInterface()->TriggerOnQueryForAvailablePurchasesCompleteDelegates(bWasSuccessful);
}


void FOnlineAsyncTaskGooglePlayQueryInAppPurchases::Tick()
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineAsyncTaskGooglePlayQueryInAppPurchases::Tick"));
	bWasSuccessful = false;

	extern bool AndroidThunkCpp_Iap_QueryInAppPurchases(const TArray<FString>&, const TArray<bool>&);
	AndroidThunkCpp_Iap_QueryInAppPurchases(ProductIds, IsConsumableFlags);
}

