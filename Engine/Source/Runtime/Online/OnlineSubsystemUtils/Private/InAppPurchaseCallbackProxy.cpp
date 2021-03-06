// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemUtilsPrivatePCH.h"

//////////////////////////////////////////////////////////////////////////
// UInAppPurchaseCallbackProxy

UInAppPurchaseCallbackProxy::UInAppPurchaseCallbackProxy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PurchaseRequest = nullptr;
	WorldPtr = nullptr;
    SavedPurchaseReceipt = nullptr;
}


void UInAppPurchaseCallbackProxy::Trigger(APlayerController* PlayerController, const FString& ProductIdentifier)
{
	bFailedToEvenSubmit = true;

	WorldPtr = (PlayerController != nullptr) ? PlayerController->GetWorld() : nullptr;
	if (APlayerState* PlayerState = (PlayerController != nullptr) ? PlayerController->PlayerState : nullptr)
	{
		if (IOnlineSubsystem* const OnlineSub = IOnlineSubsystem::Get())
		{
			IOnlineStorePtr StoreInterface = OnlineSub->GetStoreInterface();
			if (StoreInterface.IsValid())
			{
				bFailedToEvenSubmit = false;

				// Register the completion callback
				InAppPurchaseCompleteDelegate = FOnInAppPurchaseCompleteDelegate::CreateUObject(this, &UInAppPurchaseCallbackProxy::OnInAppPurchaseComplete);
				StoreInterface->AddOnInAppPurchaseCompleteDelegate(InAppPurchaseCompleteDelegate);

				// Set-up, and trigger the transaction through the store interface
				PurchaseRequest = MakeShareable(new FOnlineInAppPurchaseTransaction());
				FOnlineInAppPurchaseTransactionRef PurchaseRequestRef = PurchaseRequest.ToSharedRef();
				StoreInterface->BeginPurchase(ProductIdentifier, PurchaseRequestRef);
			}
			else
			{
				FFrame::KismetExecutionMessage(TEXT("UInAppPurchaseCallbackProxy::Trigger - In-App Purchases are not supported by Online Subsystem"), ELogVerbosity::Warning);
			}
		}
		else
		{
			FFrame::KismetExecutionMessage(TEXT("UInAppPurchaseCallbackProxy::Trigger - Invalid or uninitialized OnlineSubsystem"), ELogVerbosity::Warning);
		}
	}
	else
	{
		FFrame::KismetExecutionMessage(TEXT("UInAppPurchaseCallbackProxy::Trigger - Invalid player state"), ELogVerbosity::Warning);
	}

	if (bFailedToEvenSubmit && (PlayerController != NULL))
	{
		OnInAppPurchaseComplete(EInAppPurchaseState::Failed);
	}
}


void UInAppPurchaseCallbackProxy::OnInAppPurchaseComplete(EInAppPurchaseState::Type CompletionState, const IPlatformPurchaseReceipt* ProductReceipt)
{
	RemoveDelegate();
	SavedPurchaseState = CompletionState;
    SavedPurchaseReceipt = ProductReceipt;
    
	if (UWorld* World = WorldPtr.Get())
	{
		World->GetTimerManager().SetTimer(this, &UInAppPurchaseCallbackProxy::OnInAppPurchaseComplete_Delayed, 0.001f, false);
    }
    else
    {
        PurchaseRequest = nullptr;
    }
}


void UInAppPurchaseCallbackProxy::OnInAppPurchaseComplete_Delayed()
{
    /** Cached product details of the purchased product */
    FInAppPurchaseProductInfo ProductInformation;
    
    if (SavedPurchaseState == EInAppPurchaseState::Success && PurchaseRequest.IsValid())
    {
        ProductInformation = PurchaseRequest->ProvidedProductInformation;
    }
    
	if (SavedPurchaseState == EInAppPurchaseState::Success)
	{
		OnSuccess.Broadcast(SavedPurchaseState, ProductInformation);
	}
	else
	{
		OnFailure.Broadcast(SavedPurchaseState, ProductInformation);
	}
    
    SavedPurchaseReceipt = nullptr;
    PurchaseRequest = nullptr;
}


void UInAppPurchaseCallbackProxy::RemoveDelegate()
{
	if (!bFailedToEvenSubmit)
	{
		if (IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get())
		{
			IOnlineStorePtr InAppPurchases = OnlineSub->GetStoreInterface();
			if (InAppPurchases.IsValid())
			{
				InAppPurchases->ClearOnInAppPurchaseCompleteDelegate(InAppPurchaseCompleteDelegate);
			}
		}
	}
}


void UInAppPurchaseCallbackProxy::BeginDestroy()
{
	PurchaseRequest = nullptr;
	RemoveDelegate();

	Super::BeginDestroy();
}


UInAppPurchaseCallbackProxy* UInAppPurchaseCallbackProxy::CreateProxyObjectForInAppPurchase(class APlayerController* PlayerController, const FString& ProductIdentifier)
{
	UInAppPurchaseCallbackProxy* Proxy = NewObject<UInAppPurchaseCallbackProxy>();
	Proxy->Trigger(PlayerController, ProductIdentifier);
	return Proxy;
}
