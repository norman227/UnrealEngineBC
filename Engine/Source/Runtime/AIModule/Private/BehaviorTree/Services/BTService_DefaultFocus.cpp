// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/Services/BTService_DefaultFocus.h"
#include "BehaviorTree/Blackboard/BlackboardKeyAllTypes.h"

UBTService_DefaultFocus::UBTService_DefaultFocus(const FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer)
{
	NodeName = "Set default focus";

	bNotifyTick = false;
	bTickIntervals = false;
	bNotifyBecomeRelevant = true;
	bNotifyCeaseRelevant = true;

	// accept only actors and vectors
	BlackboardKey.AddObjectFilter(this, AActor::StaticClass());
	BlackboardKey.AddVectorFilter(this);
}

void UBTService_DefaultFocus::OnBecomeRelevant(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory) 
{
	check(OwnerComp);

	Super::OnBecomeRelevant(OwnerComp, NodeMemory);

	FBTFocusMemory* MyMemory = (FBTFocusMemory*)NodeMemory;
	check(MyMemory);
	MyMemory->Reset();

	AAIController* OwnerController = OwnerComp->GetAIOwner();
	const UBlackboardComponent* MyBlackboard = OwnerComp->GetBlackboardComponent();
	
	if (OwnerController != NULL && MyBlackboard != NULL)
	{
		if (BlackboardKey.SelectedKeyType == UBlackboardKeyType_Object::StaticClass())
		{
			UObject* KeyValue = MyBlackboard->GetValueAsObject(BlackboardKey.GetSelectedKeyID());
			AActor* TargetActor = Cast<AActor>(KeyValue);
			if (TargetActor)
			{
				OwnerController->SetFocus(TargetActor, EAIFocusPriority::Default);
				MyMemory->FocusActorSet = TargetActor;
				MyMemory->bActorSet = true;
			}
		}
		else
		{
			const FVector FocusLocation = MyBlackboard->GetValueAsVector(BlackboardKey.GetSelectedKeyID());
			OwnerController->SetFocalPoint(FocusLocation, /*bOffsetFromBase=*/false, EAIFocusPriority::Default);
			MyMemory->FocusLocationSet = FocusLocation;
		}
	}
}

void UBTService_DefaultFocus::OnCeaseRelevant(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory)
{
	check(OwnerComp);

	Super::OnCeaseRelevant(OwnerComp, NodeMemory);

	FBTFocusMemory* MyMemory = (FBTFocusMemory*)NodeMemory;
	check(MyMemory);
	AAIController* OwnerController = OwnerComp->GetAIOwner();
	if (OwnerController != NULL)
	{
		bool bClearFocus = false;
		if (MyMemory->bActorSet)
		{
			bClearFocus = (MyMemory->FocusActorSet == OwnerController->GetFocusActor(EAIFocusPriority::Default));
		}
		else
		{
			bClearFocus = (MyMemory->FocusLocationSet == OwnerController->GetFocalPoint(EAIFocusPriority::Default));
		}

		if (bClearFocus)
		{
			OwnerController->ClearFocus(EAIFocusPriority::Default);
		}
	}
}

FString UBTService_DefaultFocus::GetStaticDescription() const
{	
	FString KeyDesc("invalid");
	if (BlackboardKey.SelectedKeyType == UBlackboardKeyType_Object::StaticClass() ||
		BlackboardKey.SelectedKeyType == UBlackboardKeyType_Vector::StaticClass())
	{
		KeyDesc = BlackboardKey.SelectedKeyName.ToString();
	}

	return FString::Printf(TEXT("Set default focus to %s"), *KeyDesc);
}

void UBTService_DefaultFocus::DescribeRuntimeValues(const UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const
{
	Super::DescribeRuntimeValues(OwnerComp, NodeMemory, Verbosity, Values);
}

#if WITH_EDITOR

FName UBTService_DefaultFocus::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Service.DefaultFocus.Icon");
}

#endif	// WITH_EDITOR