// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "SoundDefinitions.h"
#include "Sound/ReverbVolume.h"
#include "BehaviorTree/Tasks/BTTask_MakeNoise.h"

UBTTask_MakeNoise::UBTTask_MakeNoise(const class FPostConstructInitializeProperties& PCIP) 
	: Super(PCIP)
	, Loudnes(1.0f)
{
	NodeName = "MakeNoise";
}

EBTNodeResult::Type UBTTask_MakeNoise::ExecuteTask(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory)
{
	const AAIController* MyController = OwnerComp ? Cast<AAIController>(OwnerComp->GetOwner()) : NULL;
	APawn* MyPawn = MyController ? MyController->GetPawn() : NULL;

	if (MyPawn)
	{
		MyPawn->MakeNoise(Loudnes, MyPawn);
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}

#if WITH_EDITOR

FName UBTTask_MakeNoise::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Task.MakeNoise.Icon");
}

#endif	// WITH_EDITOR