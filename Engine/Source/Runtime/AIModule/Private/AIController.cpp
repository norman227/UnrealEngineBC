// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "Kismet/GameplayStatics.h"
#include "DisplayDebugHelpers.h"
#include "BrainComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeManager.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Navigation/NavigationComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Engine/Canvas.h"
#include "GameFramework/PhysicsVolume.h"
#include "AIController.h"

// mz@todo these need to be removed, legacy code
#define CLOSEPROXIMITY					500.f
#define NEARSIGHTTHRESHOLD				2000.f
#define MEDSIGHTTHRESHOLD				3162.f
#define FARSIGHTTHRESHOLD				8000.f
#define CLOSEPROXIMITYSQUARED			(CLOSEPROXIMITY*CLOSEPROXIMITY)
#define NEARSIGHTTHRESHOLDSQUARED		(NEARSIGHTTHRESHOLD*NEARSIGHTTHRESHOLD)
#define MEDSIGHTTHRESHOLDSQUARED		(MEDSIGHTTHRESHOLD*MEDSIGHTTHRESHOLD)
#define FARSIGHTTHRESHOLDSQUARED		(FARSIGHTTHRESHOLD*FARSIGHTTHRESHOLD)


bool AAIController::bAIIgnorePlayers = false;

DECLARE_CYCLE_STAT(TEXT("MoveToLocation"),STAT_MoveToLocation,STATGROUP_AI);
DECLARE_CYCLE_STAT(TEXT("MoveToActor"),STAT_MoveToActor,STATGROUP_AI);

DEFINE_LOG_CATEGORY(LogAINavigation);

AAIController::AAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// set up navigation component
	NavComponent = ObjectInitializer.CreateDefaultSubobject<UNavigationComponent>(this, TEXT("NavComponent"));

	PathFollowingComponent = ObjectInitializer.CreateDefaultSubobject<UPathFollowingComponent>(this, TEXT("PathFollowingComponent"));
	PathFollowingComponent->OnMoveFinished.AddUObject(this, &AAIController::OnMoveCompleted);

	ActionsComp = ObjectInitializer.CreateDefaultSubobject<UPawnActionsComponent>(this, "ActionsComp");

	bSkipExtraLOSChecks = true;
	bWantsPlayerState = false;
}

void AAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateControlRotation(DeltaTime);
}

void AAIController::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (bWantsPlayerState && !IsPendingKill() && (GetNetMode() != NM_Client))
	{
		InitPlayerState();
	}
}

void AAIController::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	// cache PerceptionComponent if not already set
	// note that it's possible for an AI to not have a perception component at all
	if (PerceptionComponent == NULL || PerceptionComponent->IsPendingKill() == true)
	{
		PerceptionComponent = FindComponentByClass<UAIPerceptionComponent>();
	}
}

void AAIController::Reset()
{
	Super::Reset();

	if (PathFollowingComponent)
	{
		PathFollowingComponent->AbortMove(TEXT("controller reset"));
	}
}

void AAIController::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	Super::DisplayDebug(Canvas, DebugDisplay, YL, YPos);

	static FName NAME_AI = FName(TEXT("AI"));
	if (DebugDisplay.IsDisplayOn(NAME_AI))
	{
		if (PathFollowingComponent)
		{
			PathFollowingComponent->DisplayDebug(Canvas, DebugDisplay, YL, YPos);
		}
		
		if ( GetFocusActor() )
		{
			Canvas->DrawText(GEngine->GetSmallFont(), FString::Printf(TEXT("      Focus %s"), *GetFocusActor()->GetName()), 4.0f, YPos);
			YPos += YL;
		}
	}
}

#if ENABLE_VISUAL_LOG

void AAIController::GrabDebugSnapshot(FVisualLogEntry* Snapshot) const
{
	FVisualLogStatusCategory MyCategory;
	MyCategory.Category = TEXT("AI Controller");
	MyCategory.Add(TEXT("Pawn"), GetNameSafe(GetPawn()));
	AActor* FocusActor = GetFocusActor();
	MyCategory.Add(TEXT("Focus"), GetDebugName(FocusActor));
	if (FocusActor == NULL)
	{
		MyCategory.Add(TEXT("Focus Location"), GetFocalPoint().ToString());
	}
	Snapshot->Status.Add(MyCategory);

	if (GetPawn())
	{
		Snapshot->Location = GetPawn()->GetActorLocation();
	}

	if (NavComponent)
	{
		NavComponent->DescribeSelfToVisLog(Snapshot);
	}

	if (PathFollowingComponent)
	{
		PathFollowingComponent->DescribeSelfToVisLog(Snapshot);
	}

	if (BrainComponent != NULL)
	{
		BrainComponent->DescribeSelfToVisLog(Snapshot);
	}

	if (PerceptionComponent != NULL)
	{
		PerceptionComponent->DescribeSelfToVisLog(Snapshot);
	}
}
#endif // ENABLE_VISUAL_LOG

void AAIController::GetPlayerViewPoint(FVector& out_Location, FRotator& out_Rotation) const
{
	// AI does things from the Pawn
	if (GetPawn() != NULL)
	{
		out_Location = GetPawn()->GetActorLocation();
		out_Rotation = GetPawn()->GetActorRotation();
	}
	else
	{
		Super::GetPlayerViewPoint(out_Location, out_Rotation);
	}
}

void AAIController::SetFocalPoint( FVector FP, bool bOffsetFromBase, uint8 InPriority)
{
	if (InPriority >= FocusInformation.Priorities.Num())
	{
		FocusInformation.Priorities.SetNum(InPriority+1);
	}
	FFocusKnowledge::FFocusItem& Focusitem = FocusInformation.Priorities[InPriority];

	{
		AActor* FocalActor = NULL;
		if (bOffsetFromBase)
		{
			APawn *Pawn = GetPawn();
			if (Pawn && Pawn->GetMovementBase())
			{
				FocalActor = Pawn->GetMovementBase()->GetOwner();
			}
		}
		Focusitem.Position.Set( FocalActor, FP );
	}

	Focusitem.Actor = NULL;
}

FVector AAIController::GetFocalPoint(EAIFocusPriority::Type Priority) const
{	
	FBasedPosition FocalPointForPriority;

	const FFocusKnowledge::FFocusItem& FocusItem = GetFocusItem(Priority);

	if (FocusItem.Actor.IsValid())
	{
		const AActor* Focus = FocusItem.Actor.Get();
		UPrimitiveComponent* MyBase = GetPawn() ? GetPawn()->GetMovementBase() : NULL;
		const bool bRequestedFocusIsBased = MyBase && Cast<const APawn>(Focus) && (Cast<const APawn>(Focus)->GetMovementBase() == MyBase);
		FocalPointForPriority.Set(bRequestedFocusIsBased && MyBase ? MyBase->GetOwner() : NULL, Focus->GetActorLocation());
	}
	else if (!(*FocusItem.Position).IsZero())
	{
		FocalPointForPriority = FocusItem.Position;
	}

	return *FocalPointForPriority;
}

FVector AAIController::GetFocalPoint() const
{
	FBasedPosition FinalFocus;

	// find focus with highest priority
	for( int32 Index = FocusInformation.Priorities.Num()-1; Index >= 0; --Index)
	{
		const FFocusKnowledge::FFocusItem& Focusitem = FocusInformation.Priorities[Index];
		if ( Focusitem.Actor.IsValid() )
		{
			const AActor* Focus = Focusitem.Actor.Get();
			UPrimitiveComponent* MyBase = GetPawn() ? GetPawn()->GetMovementBase() : NULL;
			const bool bRequestedFocusIsBased = MyBase && Cast<const APawn>(Focus) && (Cast<const APawn>(Focus)->GetMovementBase() == MyBase);
			FinalFocus.Set(bRequestedFocusIsBased && MyBase ? MyBase->GetOwner() : NULL, Focus->GetActorLocation());
			break;
		}
		else if( !(*Focusitem.Position).IsZero() )
		{
			FinalFocus = Focusitem.Position;
			break;
		}
	}

	return *FinalFocus;
}

AActor* AAIController::GetFocusActor() const
{
	AActor* FocusActor = NULL;
	for( int32 Index = FocusInformation.Priorities.Num()-1; Index >= 0; --Index)
	{
		const FFocusKnowledge::FFocusItem& Focusitem = FocusInformation.Priorities[Index];
		if ( Focusitem.Actor.IsValid() )
		{
			FocusActor = Focusitem.Actor.Get();
			break;
		}
		else if( !(*Focusitem.Position).IsZero() )
		{
			break;
		}
	}

	return FocusActor;
}

void AAIController::K2_SetFocus(AActor* NewFocus)
{ 
	SetFocus(NewFocus, EAIFocusPriority::Gameplay); 
}

void AAIController::K2_SetFocalPoint(FVector FP, bool bOffsetFromBase)
{
	SetFocalPoint(FP, bOffsetFromBase, EAIFocusPriority::Gameplay);
}

void AAIController::K2_ClearFocus()
{
	ClearFocus(EAIFocusPriority::Gameplay);
}

void AAIController::SetFocus(AActor* NewFocus, EAIFocusPriority::Type InPriority)
{
	if( NewFocus )
	{
		if (InPriority >= FocusInformation.Priorities.Num())
		{
			FocusInformation.Priorities.SetNum(InPriority+1);
		}
		FocusInformation.Priorities[InPriority].Actor = NewFocus;
	}
	else
	{
		ClearFocus(InPriority);
	}
}

void AAIController::ClearFocus(EAIFocusPriority::Type InPriority)
{
	if (InPriority < FocusInformation.Priorities.Num())
	{
		FocusInformation.Priorities[InPriority].Actor = NULL;
		FocusInformation.Priorities[InPriority].Position.Clear();
	}
}

bool AAIController::LineOfSightTo(const AActor* Other, FVector ViewPoint, bool bAlternateChecks) const
{
	if( !Other )
	{
		return false;
	}
		
	if ( ViewPoint.IsZero() )
	{
		AActor*	ViewTarg = GetViewTarget();
		ViewPoint = ViewTarg->GetActorLocation();
		if( ViewTarg == GetPawn() )
		{
			ViewPoint.Z += GetPawn()->BaseEyeHeight; //look from eyes
		}
	}

	static FName NAME_LineOfSight = FName(TEXT("LineOfSight"));
	FVector TargetLocation = Other->GetTargetLocation(GetPawn());

	FCollisionQueryParams CollisionParams(NAME_LineOfSight, true, this->GetPawn());
	CollisionParams.AddIgnoredActor(Other);

	bool bHit = GetWorld()->LineTraceTest(ViewPoint, TargetLocation, ECC_Visibility, CollisionParams);
	if( !bHit )
	{
		return true;
	}

	// if other isn't using a cylinder for collision and isn't a Pawn (which already requires an accurate cylinder for AI)
	// then don't go any further as it likely will not be tracing to the correct location
	const APawn * OtherPawn = Cast<const APawn>(Other);
	if (!OtherPawn && Cast<UCapsuleComponent>(Other->GetRootComponent()) == NULL)
	{
		return false;
	}

	const FVector OtherActorLocation = Other->GetActorLocation();
	const float DistSq = (OtherActorLocation - ViewPoint).SizeSquared();
	if ( DistSq > FARSIGHTTHRESHOLDSQUARED )
	{
		return false;
	}

	if ( !OtherPawn && (DistSq > NEARSIGHTTHRESHOLDSQUARED) ) 
	{
		return false;
	}

	float OtherRadius, OtherHeight;
	Other->GetSimpleCollisionCylinder(OtherRadius, OtherHeight);

	if ( !bAlternateChecks || !bLOSflag )
	{
		//try viewpoint to head
		bHit = GetWorld()->LineTraceTest(ViewPoint,  OtherActorLocation + FVector(0.f,0.f,OtherHeight), ECC_Visibility, CollisionParams);
		if ( !bHit )
		{
			return true;
		}
	}

	if( !bSkipExtraLOSChecks && (!bAlternateChecks || bLOSflag) )
	{
		// only check sides if width of other is significant compared to distance
		if( OtherRadius * OtherRadius/(OtherActorLocation - ViewPoint).SizeSquared() < 0.0001f )
		{
			return false;
		}
		//try checking sides - look at dist to four side points, and cull furthest and closest
		FVector Points[4];
		Points[0] = OtherActorLocation - FVector(OtherRadius, -1 * OtherRadius, 0);
		Points[1] = OtherActorLocation + FVector(OtherRadius, OtherRadius, 0);
		Points[2] = OtherActorLocation - FVector(OtherRadius, OtherRadius, 0);
		Points[3] = OtherActorLocation + FVector(OtherRadius, -1 * OtherRadius, 0);
		int32 IndexMin = 0;
		int32 IndexMax = 0;
		float CurrentMax = (Points[0] - ViewPoint).SizeSquared();
		float CurrentMin = CurrentMax;
		for ( int32 PointIndex=1; PointIndex<4; PointIndex++ )
		{
			const float NextSize = (Points[PointIndex] - ViewPoint).SizeSquared();
			if (NextSize > CurrentMin)
			{
				CurrentMin = NextSize;
				IndexMax = PointIndex;
			}
			else if (NextSize < CurrentMax)
			{
				CurrentMax = NextSize;
				IndexMin = PointIndex;
			}
		}

		for ( int32 PointIndex=0; PointIndex<4; PointIndex++ )
		{
			if ((PointIndex != IndexMin) && (PointIndex != IndexMax))
			{
				bHit = GetWorld()->LineTraceTest(ViewPoint,  Points[PointIndex], ECC_Visibility, CollisionParams);
				if ( !bHit )
				{
					return true;
				}
			}
		}
	}
	return false;
}

void AAIController::ActorsPerceptionUpdated(const TArray<AActor*>& UpdatedActors)
{

}

DEFINE_LOG_CATEGORY_STATIC(LogTestAI, All, All);

void AAIController::UpdateControlRotation(float DeltaTime, bool bUpdatePawn)
{
	// Look toward focus
	FVector FocalPoint = GetFocalPoint();
	if( !FocalPoint.IsZero() && GetPawn())
	{
		FVector Direction = FocalPoint - GetPawn()->GetActorLocation();
		FRotator NewControlRotation = Direction.Rotation();

		// Don't pitch view of walking pawns unless looking at another pawn
		if ( GetPawn()->GetMovementComponent() && GetPawn()->GetMovementComponent()->IsMovingOnGround() &&
			PathFollowingComponent && (!PathFollowingComponent->GetMoveGoal() || !Cast<APawn>(PathFollowingComponent->GetMoveGoal()) ) )
		{
			NewControlRotation.Pitch = 0.f;
		}
		NewControlRotation.Yaw = FRotator::ClampAxis(NewControlRotation.Yaw);

		if (!GetControlRotation().Equals(NewControlRotation, 1e-3f))
		{
			SetControlRotation(NewControlRotation);

			APawn* const P = GetPawn();
			if (P && bUpdatePawn)
			{
				P->FaceRotation(NewControlRotation, DeltaTime);
			}
		}
	}
}


void AAIController::Possess(APawn* InPawn)
{
	Super::Possess(InPawn);

	if (!GetPawn())
	{
		return;
	}
	
	// no point in doing navigation setup if pawn has no movement component
	const UPawnMovementComponent* MovementComp = InPawn->GetMovementComponent();
	if (MovementComp != NULL)
	{
		UpdateNavigationComponents();
	}

	if (PathFollowingComponent)
	{
		PathFollowingComponent->Initialize();
	}

	if (bWantsPlayerState)
	{
		ChangeState(NAME_Playing);
	}

	OnPossess(InPawn);
}

void AAIController::UnPossess()
{
	Super::UnPossess();

	if (PathFollowingComponent)
	{
		PathFollowingComponent->Cleanup();
	}
}

void AAIController::InitNavigationControl(UNavigationComponent*& PathFindingComp, UPathFollowingComponent*& PathFollowingComp)
{
	PathFindingComp = NavComponent;
	PathFollowingComp = PathFollowingComponent;
}

EPathFollowingRequestResult::Type AAIController::MoveToActor(AActor* Goal, float AcceptanceRadius, bool bStopOnOverlap, bool bUsePathfinding, bool bCanStrafe, TSubclassOf<UNavigationQueryFilter> FilterClass)
{
	SCOPE_CYCLE_COUNTER(STAT_MoveToActor);
	EPathFollowingRequestResult::Type Result = EPathFollowingRequestResult::Failed;

	UE_VLOG(this, LogAINavigation, Log, TEXT("MoveToActor: Goal(%s) AcceptRadius(%.1f%s) bUsePathfinding(%d) bCanStrafe(%d)"),
		*GetNameSafe(Goal), AcceptanceRadius, bStopOnOverlap ? TEXT(" + agent") : TEXT(""), bUsePathfinding, bCanStrafe);

	if (Goal)
	{
		if (PathFollowingComponent && PathFollowingComponent->HasReached(Goal, AcceptanceRadius, !bStopOnOverlap))
		{
			UE_VLOG(this, LogAINavigation, Log, TEXT("MoveToActor: already at goal!"));

			// make sure previous move request gets aborted
			PathFollowingComponent->AbortMove(TEXT("Aborting move due to new move request finishing with AlreadyAtGoal"), FAIRequestID::AnyRequest);

			PathFollowingComponent->SetLastMoveAtGoal(true);

			OnMoveCompleted(FAIRequestID::CurrentRequest, EPathFollowingResult::Success);
			Result = EPathFollowingRequestResult::AlreadyAtGoal;
		}
		else
		{
			FNavPathSharedPtr Path = FindPath(Goal, bUsePathfinding, UNavigationQueryFilter::GetQueryFilter(NavComponent ? NavComponent->GetNavData() : NULL, FilterClass));
			if (Path.IsValid())
			{
				bAllowStrafe = bCanStrafe;

				const uint32 RequestID = RequestMove(Path, Goal, AcceptanceRadius, bStopOnOverlap);
				Result = (RequestID != FAIRequestID::InvalidRequest) ? EPathFollowingRequestResult::RequestSuccessful : EPathFollowingRequestResult::Failed;
			}
		}
	}

	if (Result == EPathFollowingRequestResult::Failed)
	{
		if (PathFollowingComponent)
		{
			PathFollowingComponent->SetLastMoveAtGoal(false);
		}

		OnMoveCompleted(FAIRequestID::InvalidRequest, EPathFollowingResult::Invalid);
	}

	return Result;
}

EPathFollowingRequestResult::Type AAIController::MoveToLocation(const FVector& Dest, float AcceptanceRadius, bool bStopOnOverlap, bool bUsePathfinding, bool bProjectDestinationToNavigation, bool bCanStrafe, TSubclassOf<UNavigationQueryFilter> FilterClass)
{
	SCOPE_CYCLE_COUNTER(STAT_MoveToLocation);

	EPathFollowingRequestResult::Type Result = EPathFollowingRequestResult::Failed;
	bool bCanRequestMove = true;

	UE_VLOG(this, LogAINavigation, Log, TEXT("MoveToLocation: Goal(%s) AcceptRadius(%.1f%s) bUsePathfinding(%d) bCanStrafe(%d)"),
		*Dest.ToString(), AcceptanceRadius, bStopOnOverlap ? TEXT(" + agent") : TEXT(""), bUsePathfinding, bCanStrafe);

	// Check input is valid
	if( Dest.ContainsNaN() )
	{
		UE_VLOG(this, LogAINavigation, Error, TEXT("AAIController::MoveToLocation: Destination is not valid! Goal(%s) AcceptRadius(%.1f%s) bUsePathfinding(%d) bCanStrafe(%d)"),
			*Dest.ToString(), AcceptanceRadius, bStopOnOverlap ? TEXT(" + agent") : TEXT(""), bUsePathfinding, bCanStrafe);
		
		ensure( !Dest.ContainsNaN() );
		bCanRequestMove = false;
	}

	FVector GoalLocation = Dest;

	// fail if projection to navigation is required but it either failed or there's not navigation component
	if (bCanRequestMove && bProjectDestinationToNavigation && (NavComponent == NULL || !NavComponent->ProjectPointToNavigation(Dest, GoalLocation)))
	{
		UE_VLOG_LOCATION(this, LogAINavigation, Error, Dest, 30.f, FLinearColor::Red, TEXT("AAIController::MoveToLocation failed to project destination location to navmesh"));
		bCanRequestMove = false;
	}

	if (bCanRequestMove && PathFollowingComponent && PathFollowingComponent->HasReached(GoalLocation, AcceptanceRadius, bStopOnOverlap))
	{
		UE_VLOG(this, LogAINavigation, Log, TEXT("MoveToLocation: already at goal!"));

		// make sure previous move request gets aborted
		PathFollowingComponent->AbortMove(TEXT("Aborting move due to new move request finishing with AlreadyAtGoal"), FAIRequestID::AnyRequest);

		PathFollowingComponent->SetLastMoveAtGoal(true);

		OnMoveCompleted(FAIRequestID::CurrentRequest, EPathFollowingResult::Success);
		Result = EPathFollowingRequestResult::AlreadyAtGoal;
		bCanRequestMove = false;
	}

	if (bCanRequestMove)
	{
		FNavPathSharedPtr Path = FindPath(GoalLocation, bUsePathfinding, UNavigationQueryFilter::GetQueryFilter(NavComponent != NULL ? NavComponent->GetNavData() : NULL, FilterClass));
		if (Path.IsValid())
		{
			bAllowStrafe = bCanStrafe;

			const FAIRequestID RequestID = RequestMove(Path, NULL, AcceptanceRadius, bStopOnOverlap);
			Result = RequestID.IsValid() ? EPathFollowingRequestResult::RequestSuccessful : EPathFollowingRequestResult::Failed;
		}
	}

	if (Result == EPathFollowingRequestResult::Failed)
	{
		if (PathFollowingComponent)
		{
			PathFollowingComponent->SetLastMoveAtGoal(false);
		}

		OnMoveCompleted(FAIRequestID::InvalidRequest, EPathFollowingResult::Invalid);
	}

	return Result;
}

FAIRequestID AAIController::RequestMove(FNavPathSharedPtr Path, AActor* Goal, float AcceptanceRadius, bool bStopOnOverlap, FCustomMoveSharedPtr CustomData)
{
	uint32 RequestID = FAIRequestID::InvalidRequest;
	if (PathFollowingComponent)
	{
		RequestID = PathFollowingComponent->RequestMove(Path, Goal, AcceptanceRadius, bStopOnOverlap, CustomData);
	}

	return RequestID;
}

bool AAIController::PauseMove(FAIRequestID RequestToPause)
{
	if (PathFollowingComponent != NULL && RequestToPause.IsEquivalent(PathFollowingComponent->GetCurrentRequestId()))
	{
		PathFollowingComponent->PauseMove(RequestToPause);
		return true;
	}
	return false;
}

bool AAIController::ResumeMove(FAIRequestID RequestToResume)
{
	if (PathFollowingComponent != NULL && RequestToResume.IsEquivalent(PathFollowingComponent->GetCurrentRequestId()))
	{
		PathFollowingComponent->ResumeMove(RequestToResume);
		return true;
	}
	return false;
}

void AAIController::StopMovement()
{
	UE_VLOG(this, LogAINavigation, Log, TEXT("AAIController::StopMovement: %s STOP MOVEMENT"), *GetNameSafe(GetPawn()));
	PathFollowingComponent->AbortMove(TEXT("StopMovement"));
}

FNavPathSharedPtr AAIController::FindPath(AActor* Goal, bool bUsePathfinding, TSharedPtr<const FNavigationQueryFilter> QueryFilter)
{
	if (NavComponent)
	{
		const bool bFoundPath = bUsePathfinding ? NavComponent->FindPathToActor(Goal, QueryFilter) : NavComponent->FindSimplePathToActor(Goal);
		if (bFoundPath)
		{
			return NavComponent->GetPath();
		}
	}

	return NULL;
}

FNavPathSharedPtr AAIController::FindPath(const FVector& Dest, bool bUsePathfinding, TSharedPtr<const FNavigationQueryFilter> QueryFilter)
{
	if (NavComponent)
	{
		const bool bFoundPath = bUsePathfinding ? NavComponent->FindPathToLocation(Dest, QueryFilter) : NavComponent->FindSimplePathToLocation(Dest);
		if (bFoundPath) 
		{
			return NavComponent->GetPath();
		}
	}

	return NULL;
}

EPathFollowingStatus::Type AAIController::GetMoveStatus() const
{
	return (PathFollowingComponent) ? PathFollowingComponent->GetStatus() : EPathFollowingStatus::Idle;
}

bool AAIController::HasPartialPath() const
{
	return (PathFollowingComponent != NULL) && (PathFollowingComponent->HasPartialPath());
}

FVector AAIController::GetImmediateMoveDestination() const
{
	return (PathFollowingComponent) ? PathFollowingComponent->GetCurrentTargetLocation() : FVector::ZeroVector;
}

void AAIController::SetMoveBlockDetection(bool bEnable)
{
	if (PathFollowingComponent)
	{
		PathFollowingComponent->SetBlockDetectionState(bEnable);
	}
}

void AAIController::OnMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result)
{
	ReceiveMoveCompleted.Broadcast(RequestID, Result);
}

bool AAIController::RunBehaviorTree(UBehaviorTree* BTAsset)
{
	// @todo: find BrainComponent and see if it's BehaviorTreeComponent
	// Also check if BTAsset requires BlackBoardComponent, and if so 
	// check if BB type is accepted by BTAsset.
	// Spawn BehaviorTreeComponent if none present. 
	// Spawn BlackBoardComponent if none present, but fail if one is present but is not of compatible class
	if (BTAsset == NULL)
	{
		UE_VLOG(this, LogBehaviorTree, Warning, TEXT("RunBehaviorTree: Unable to run NULL behavior tree"));
		return false;
	}

	bool bSuccess = true;
	bool bShouldInitializeBlackboard = false;

	// see if need a blackboard component at all
	UBlackboardComponent* BlackboardComp = NULL;
	if (BTAsset->BlackboardAsset)
	{
		bSuccess = UseBlackboard(BTAsset->BlackboardAsset);
		BlackboardComp = FindComponentByClass<UBlackboardComponent>();
	}
	
	if (bSuccess)
	{
		UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(BrainComponent);
		if (BTComp == NULL)
		{
			UE_VLOG(this, LogBehaviorTree, Log, TEXT("RunBehaviorTree: spawning BehaviorTreeComponent.."));

			BrainComponent = BTComp = ConstructObject<UBehaviorTreeComponent>(UBehaviorTreeComponent::StaticClass(), this, TEXT("BTComponent"));
			BrainComponent->RegisterComponent();
		}

		check(BTComp != NULL);
		BTComp->StartTree(*BTAsset, EBTExecutionMode::Looped);
	}

	return bSuccess;
}

bool AAIController::UseBlackboard(UBlackboardData* BlackboardAsset)
{
	if (BlackboardAsset == NULL)
	{
		UE_VLOG(this, LogBehaviorTree, Log, TEXT("UseBlackboard: trying to use NULL Blackboard asset. Ignoring"));
		return false;
	}

	bool bSuccess = true;
	UBlackboardComponent* BlackboardComp = FindComponentByClass<UBlackboardComponent>();

	if (BlackboardComp == NULL)
	{
		BlackboardComp = ConstructObject<UBlackboardComponent>(UBlackboardComponent::StaticClass(), this, TEXT("BlackboardComponent"));
		if (BlackboardComp != NULL)
		{
			BlackboardComp->InitializeBlackboard(BlackboardAsset);
			BlackboardComp->RegisterComponent();
		}

	}
	else if (BlackboardComp->GetBlackboardAsset() == NULL)
	{
		BlackboardComp->InitializeBlackboard(BlackboardAsset);
	}
	else if (BlackboardComp->GetBlackboardAsset() != BlackboardAsset)
	{
		UE_VLOG(this, LogBehaviorTree, Log, TEXT("UseBlackboard: requested blackboard %s while already has %s instantiated. Forcing new BB.")
			, *GetNameSafe(BlackboardAsset), *GetNameSafe(BlackboardComp->GetBlackboardAsset()));
		BlackboardComp->InitializeBlackboard(BlackboardAsset);
	}

	return bSuccess;
}

bool AAIController::SuggestTossVelocity(FVector& OutTossVelocity, FVector Start, FVector End, float TossSpeed, bool bPreferHighArc, float CollisionRadius, bool bOnlyTraceUp)
{
	// pawn's physics volume gets 2nd priority
	APhysicsVolume const* const PhysicsVolume = GetPawn() ? GetPawn()->GetPawnPhysicsVolume() : NULL;
	float const GravityOverride = PhysicsVolume ? PhysicsVolume->GetGravityZ() : 0.f;
	ESuggestProjVelocityTraceOption::Type const TraceOption = bOnlyTraceUp ? ESuggestProjVelocityTraceOption::OnlyTraceWhileAsceding : ESuggestProjVelocityTraceOption::TraceFullPath;

	return UGameplayStatics::SuggestProjectileVelocity(this, OutTossVelocity, Start, End, TossSpeed, bPreferHighArc, CollisionRadius, GravityOverride, TraceOption);
}
bool AAIController::PerformAction(UPawnAction& Action, EAIRequestPriority::Type Priority, UObject* const Instigator /*= NULL*/)
{
	return ActionsComp != NULL && ActionsComp->PushAction(Action, Priority, Instigator);
}

FString AAIController::GetDebugIcon() const
{
	if (BrainComponent == NULL || BrainComponent->IsRunning() == false)
	{
		return TEXT("/Engine/EngineResources/AICON-Red.AICON-Red");
	}

	return TEXT("/Engine/EngineResources/AICON-Green.AICON-Green");
}


/** Returns NavComponent subobject **/
UNavigationComponent* AAIController::GetNavComponent() const { return NavComponent; }
/** Returns PathFollowingComponent subobject **/
UPathFollowingComponent* AAIController::GetPathFollowingComponent() const { return PathFollowingComponent; }
/** Returns ActionsComp subobject **/
UPawnActionsComponent* AAIController::GetActionsComp() const { return ActionsComp; }
