// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "AI/Navigation/NavLinkCustomComponent.h"
#include "DrawDebugHelpers.h"
#include "Navigation/NavigationComponent.h"

//----------------------------------------------------------------------//
// Life cycle                                                        
//----------------------------------------------------------------------//

UNavigationComponent::UNavigationComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bIsPathPartial(false)
	, AsynPathQueryID(INVALID_NAVQUERYID)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	
	bNotifyPathFollowing = true;
	bRepathWhenInvalid = true;
	bUpdateForSmartLinks = true;
	bWantsInitializeComponent = true;
	bAutoActivate = true;

	NavDataFlags = 0;
}

void UNavigationComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// setting up delegate here in case function used has virtual overrides
	PathObserverDelegate = FNavigationPath::FPathObserverDelegate::FDelegate::CreateUObject(this, &UNavigationComponent::OnPathEvent);
	
	UpdateCachedComponents();
}

void UNavigationComponent::UpdateCachedComponents()
{
	// try to find path following component in owning actor
	AActor* MyOwner = GetOwner();
	if (MyOwner)
	{
		PathFollowComp = MyOwner->FindComponentByClass<UPathFollowingComponent>();
		ControllerOwner = Cast<AController>(MyOwner);
	}
}

void UNavigationComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	// already requested not to tick. 
	if (bShoudTick == false)
	{
		return;
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	check( GetOuter() != NULL );

	bool bSuccess = false;

	if (GoalActor != NULL)
	{
		const FVector GoalActorLocation = GetCurrentMoveGoal(GoalActor, GetOwner());
		const float DistanceSq = FVector::DistSquared(GoalActorLocation, OriginalGoalActorLocation);

		if (DistanceSq > RepathDistanceSq)
		{
			if (RePathTo(GoalActorLocation, StoredQueryFilter))
			{
				bSuccess = true;
				OriginalGoalActorLocation = GoalActorLocation;
			}
			else
			{
				FAIMessage::Send(ControllerOwner, FAIMessage(UBrainComponent::AIMessage_RepathFailed, this));
			}
		}
		else
		{
			bSuccess = true;
		}
	}

	if (bSuccess == false)
	{
		ResetTransientData();
	}
}

void UNavigationComponent::OnPathEvent(FNavigationPath* InvalidatedPath, ENavPathEvent::Type Event)
{
	if (InvalidatedPath == NULL || InvalidatedPath != Path.Get())
    {
		return;
    }

	switch (Event)
	{
		case ENavPathEvent::Invalidated:
		{
			// try to repath automatically
			if (!bIsPaused && bRepathWhenInvalid)
			{
				RepathData.GoalActor = GoalActor;
				RepathData.GoalLocation = CurrentGoal;
				RepathData.bSimplePath = bUseSimplePath;
				RepathData.StoredQueryFilter = StoredQueryFilter;
				bIsWaitingForRepath = true;

				Path->EnableRecalculationOnInvalidation(true);

				/*
				const static TStatId StatId = TStatId::CreateAndRegisterFromName(TEXT("FSimpleDelegateGraphTask.Deferred restore path"));

				FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
					FSimpleDelegateGraphTask::FDelegate::CreateUObject(this, &UNavigationComponent::DeferredRepathToGoal),
					StatId, NULL, ENamedThreads::GameThread
				);
				*/
			}

			// reset state
			const bool bWasPaused = bIsPaused;
			//ResetTransientData();
			bIsPaused = bWasPaused;

			// and notify observer
			if (MyPathObserver && !bRepathWhenInvalid)
			{
				MyPathObserver->OnPathInvalid(this);
			}

			break;
		}
		case ENavPathEvent::RePathFailed:
			FAIMessage::Send(ControllerOwner, FAIMessage(UBrainComponent::AIMessage_RepathFailed, this));
			break;
		case ENavPathEvent::UpdatedDueToGoalMoved:
		case ENavPathEvent::UpdatedDueToNavigationChanged:
		{
			GoalActor = Path->GetGoalActor();
			CurrentGoal = Path->GetGoalLocation();
			StoredQueryFilter = Path->GetFilter();
			//bUseSimplePath = RepathData.bSimplePath;

			NotifyPathUpdate();
		}
			break;
	}
}

bool UNavigationComponent::RePathTo(const FVector& GoalLocation, TSharedPtr<const FNavigationQueryFilter> InStoredQueryFilter)
{
	return bUseSimplePath ? GenerateSimplePath(GoalLocation) :
		bDoAsyncPathfinding ? AsyncGeneratePathTo(GoalLocation, StoredQueryFilter) : GeneratePathTo(GoalLocation, InStoredQueryFilter);
}

FNavLocation UNavigationComponent::GetRandomPointOnNavMesh() const
{
#if WITH_RECAST
	ARecastNavMesh* const Nav = (ARecastNavMesh*)GetWorld()->GetNavigationSystem()->GetMainNavData(FNavigationSystem::Create);
	if (Nav)
	{
		return Nav->GetRandomPoint();
	}
#endif // WITH_RECAST
	return FNavLocation();
}

FNavLocation UNavigationComponent::ProjectPointToNavigation(const FVector& Location) const
{
	FNavLocation OutLocation(Location);
	
	if (GetNavData() != NULL)
	{
		if (!MyNavData->ProjectPoint(Location, OutLocation, GetQueryExtent()))
		{
			OutLocation = FNavLocation();
		}
	}

	return OutLocation;
}

bool UNavigationComponent::ProjectPointToNavigation(const FVector& Location, FNavLocation& ProjectedLocation) const
{
	return GetNavData() != NULL && MyNavData->ProjectPoint(Location, ProjectedLocation, GetQueryExtent());
}

bool UNavigationComponent::ProjectPointToNavigation(const FVector& Location, FVector& ProjectedLocation) const
{
	FNavLocation NavLoc;
	const bool bSuccess = ProjectPointToNavigation(Location, NavLoc);
	ProjectedLocation = NavLoc.Location;
	return bSuccess;
}

bool UNavigationComponent::AsyncGeneratePathTo(const FVector& GoalLocation, TSharedPtr<const FNavigationQueryFilter> QueryFilter)
{
	return GetOuter() && MyNavAgent && AsyncGeneratePath(MyNavAgent->GetNavAgentLocation(), GoalLocation, QueryFilter);
}

bool UNavigationComponent::AsyncGeneratePath(const FVector& FromLocation, const FVector& GoalLocation, TSharedPtr<const FNavigationQueryFilter> QueryFilter)
{
	UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
	if (NavSys == NULL || GetOuter() == NULL || MyNavAgent == NULL || GetNavData() == NULL)
	{
		return false;
	}

	// Make sure we're trying to path to the closest valid location TO that location rather than the absolute location.
	// After talking with Steve P., if we need to ever allow pathing WITHOUT projecting to nav-mesh, it will probably be
	// best to do so using a flag while keeping this as the default behavior.  NOTE: If any changes to this behavior
	// are made, you should search for other places in UNavigationComponent using NavMeshGoalLocation and make sure the
	// behavior remains consistent.
	const FNavAgentProperties* NavAgentProps = MyNavAgent->GetNavAgentProperties();
	if (NavAgentProps != NULL)
	{
		FVector NavMeshGoalLocation;

		if (ProjectPointToNavigation(GoalLocation, NavMeshGoalLocation) == QuerySuccess)
		{
			const EPathFindingMode::Type Mode = bDoHierarchicalPathfinding ? EPathFindingMode::Hierarchical : EPathFindingMode::Regular;
			FPathFindingQuery Query(GetOwner(), GetNavData(), FromLocation, NavMeshGoalLocation, QueryFilter);
			Query.NavDataFlags = NavDataFlags;

			AsynPathQueryID = NavSys->FindPathAsync(*MyNavAgent->GetNavAgentProperties(), Query,
				FNavPathQueryDelegate::CreateUObject(this, &UNavigationComponent::AsyncGeneratePath_OnCompleteCallback), Mode);
		}
		else
		{
			// projecting destination point failed
			AsynPathQueryID = INVALID_NAVQUERYID;

			UE_VLOG(GetOwner(), LogAINavigation, Warning, TEXT("AI trying to generate path to point off of navmesh"));
			UE_VLOG_LOCATION(GetOwner(), LogAINavigation, Warning, GoalLocation, 30, FColor::Red, TEXT("Invalid pathing GoalLocation"));
			if (MyNavData != NULL)
			{
				UE_VLOG_BOX(GetOwner(), LogAINavigation, Warning, FBox(GoalLocation - GetQueryExtent(), GoalLocation + GetQueryExtent()), FColor::Red, TEXT_EMPTY);
			}
		}
	}
	else
	{
		AsynPathQueryID = INVALID_NAVQUERYID;
	}

	return AsynPathQueryID != INVALID_NAVQUERYID;
}

void UNavigationComponent::Pause()
{
	bIsPaused = true;
	SetComponentTickEnabledAsync(false);
}

void UNavigationComponent::AbortAsyncFindPathRequest()
{
	if (AsynPathQueryID == INVALID_NAVQUERYID || GetWorld()->GetNavigationSystem() == NULL)
	{
		return;
	}

	GetWorld()->GetNavigationSystem()->AbortAsyncFindPathRequest((uint32)AsynPathQueryID);

	AsynPathQueryID = INVALID_NAVQUERYID;
}

void UNavigationComponent::DeferredResumePath()
{
	ResumePath();
}

bool UNavigationComponent::ResumePath()
{
	const bool bWasPaused = bIsPaused;

	bIsPaused = false;
	
	if (bWasPaused && Path.IsValid() && Path->IsValid())
	{
		if (IsFollowing() == true)
		{
			// make sure ticking is enabled (and it shouldn't be enabled before _first_ async path finding)
			//SetComponentTickEnabledAsync(true);
			Path->SetGoalActorObservation(*GoalActor, FMath::Sqrt(RepathDistanceSq));
		}

		// don't notify about path update, it's the same path...
		CurrentGoal = Path->GetPathPoints()[Path->GetPathPoints().Num() - 1].Location;
		return true;
	}

	return false;
}

void UNavigationComponent::AsyncGeneratePath_OnCompleteCallback(uint32 QueryID, ENavigationQueryResult::Type Result, FNavPathSharedPtr ResultPath)
{
	if (QueryID != uint32(AsynPathQueryID))
	{
		UE_VLOG(GetOwner(), LogAINavigation, Display, TEXT("Pathfinding query %u finished, but it's different from AsynPathQueryID (%d)"), QueryID, AsynPathQueryID);
		return;
	}

	check(GetOuter() != NULL);
	
	if (Result == ENavigationQueryResult::Success && ResultPath.IsValid() && ResultPath->IsValid())
	{
		const bool bIsSamePath = Path.IsValid() == true && ((const FNavMeshPath*)Path.Get())->ContainsWithSameEnd((const FNavMeshPath*)ResultPath.Get())
			&& FVector::DistSquared(Path->GetDestinationLocation(), ResultPath->GetDestinationLocation()) < KINDA_SMALL_NUMBER;

		if (bIsSamePath == false)
		{
			if (IsFollowing() == true)
			{
				// make sure ticking is enabled (and it shouldn't be enabled before _first_ async path finding)
				//SetComponentTickEnabledAsync(true);
				Path->SetGoalActorObservation(*GoalActor, FMath::Sqrt(RepathDistanceSq));
			}

			SetPath(ResultPath);
			CurrentGoal = ResultPath->GetPathPoints()[ResultPath->GetPathPoints().Num() - 1].Location;
			NotifyPathUpdate();
		}
		else
		{
			UE_VLOG(GetOwner(), LogAINavigation, Display, TEXT("Skipping newly generated path due to it being the same as current one"));
		}
	}
	else
	{
		ResetTransientData();
		if (MyPathObserver != NULL)
		{
			MyPathObserver->OnPathFailed(this);  
		}
	}
}

bool UNavigationComponent::GeneratePathTo(const FVector& GoalLocation, TSharedPtr<const FNavigationQueryFilter> QueryFilter)
{
	if (GetWorld() == NULL || GetWorld()->GetNavigationSystem() == NULL || GetOuter() == NULL)
	{
		return false;
	}

	// Make sure we're trying to path to the closest valid location TO that location rather than the absolute location.
	// After talking with Steve P., if we need to ever allow pathing WITHOUT projecting to nav-mesh, it will probably be
	// best to do so using a flag while keeping this as the default behavior.  NOTE:` If any changes to this behavior
	// are made, you should search for other places in UNavigationComponent using NavMeshGoalLocation and make sure the
	// behavior remains consistent.
	
	// make sure that nav data is updated
	GetNavData();

	check(MyNavAgent);

	const FNavAgentProperties* NavAgentProps = MyNavAgent->GetNavAgentProperties();
	if (NavAgentProps != NULL)
	{
		FVector NavMeshGoalLocation;

#if ENABLE_VISUAL_LOG
		UE_VLOG_LOCATION(GetOwner(), LogAINavigation, Log, GoalLocation, 34, FColor(0, 127, 14), TEXT("GoalLocation"));
		UE_VLOG_BOX(GetOwner(), LogAINavigation, Log, FBox(GoalLocation - GetQueryExtent(), GoalLocation + GetQueryExtent()), FColor::Green, TEXT_EMPTY);
#endif

		if (ProjectPointToNavigation(GoalLocation, NavMeshGoalLocation) == QuerySuccess)
		{
			UE_VLOG_SEGMENT(GetOwner(), LogAINavigation, Log, GoalLocation, NavMeshGoalLocation, FColor::Blue, TEXT_EMPTY);
			UE_VLOG_LOCATION(GetOwner(), LogAINavigation, Log, NavMeshGoalLocation, 34, FColor::Blue, TEXT_EMPTY);
			
			UNavigationSystem* NavSys = GetWorld()->GetNavigationSystem();
			FPathFindingQuery Query(GetOwner(), MyNavData, MyNavAgent->GetNavAgentLocation(), NavMeshGoalLocation, QueryFilter);
			Query.NavDataFlags = NavDataFlags;

			const EPathFindingMode::Type Mode = bDoHierarchicalPathfinding ? EPathFindingMode::Hierarchical : EPathFindingMode::Regular;
			FPathFindingResult Result = NavSys->FindPathSync(*MyNavAgent->GetNavAgentProperties(), Query, Mode);

			// try reversing direction
			if (bSearchFromGoalWhenOutOfNodes && !bDoHierarchicalPathfinding &&
				Result.Path.IsValid() && Result.Path->DidSearchReachedLimit())
			{
				// quick check if path exists
				bool bPathExists = false;
				{
					DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Pathfinding: HPA* test time"), STAT_Navigation_PathVerifyTime, STATGROUP_Navigation);
					bPathExists = NavSys->TestPathSync(Query, EPathFindingMode::Hierarchical);
				}

				UE_VLOG(GetOwner(), LogAINavigation, Log, TEXT("GeneratePathTo: out of nodes, HPA* path: %s"),
					bPathExists ? TEXT("exists, trying reversed search") : TEXT("doesn't exist"));

				// reverse search
				if (bPathExists)
				{
					DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Pathfinding: reversed test time"), STAT_Navigation_PathReverseTime, STATGROUP_Navigation);

					TSharedPtr<FNavigationQueryFilter> Filter = QueryFilter.IsValid() ? QueryFilter->GetCopy() : MyNavData->GetDefaultQueryFilter()->GetCopy();					
					Filter->SetBacktrackingEnabled(true);

					FPathFindingQuery ReversedQuery(GetOwner(), MyNavData, Query.EndLocation, Query.StartLocation, Filter);
					ReversedQuery.NavDataFlags = NavDataFlags;

					Result = NavSys->FindPathSync(*MyNavAgent->GetNavAgentProperties(), Query, Mode);
				}
			}

			if (Result.IsSuccessful() || Result.IsPartial())
			{
				CurrentGoal = NavMeshGoalLocation;
				SetPath(Result.Path);

				if (IsFollowing() == true)
				{
					// make sure ticking is enabled (and it shouldn't be enabled before _first_ async path finding)
					//SetComponentTickEnabledAsync(true);
					Path->SetGoalActorObservation(*GoalActor, FMath::Sqrt(RepathDistanceSq));
				}

				// NotifyPathUpdate();
				return true;
			}
			else
			{
				UE_VLOG(GetOwner(), LogAINavigation, Display, TEXT("Failed to generate path to destination"));
			}
		}
		else
		{
			UE_VLOG(GetOwner(), LogAINavigation, Display, TEXT("Destination not on navmesh (and nowhere near!)"));
		}
	}
	else
	{
		UE_VLOG(GetOwner(), LogAINavigation, Display, TEXT("UNavigationComponent::GeneratePathTo: NavAgentProps == NULL (Probably Pawn died)"));
	}

	return false;
}

bool UNavigationComponent::GeneratePath(const FVector& FromLocation, const FVector& GoalLocation, TSharedPtr<const FNavigationQueryFilter> QueryFilter)
{
	UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
	if (NavSys == NULL || GetOuter() == NULL || MyNavAgent == NULL || GetNavData() == NULL)
	{
		return false;
	}

	const EPathFindingMode::Type Mode = bDoHierarchicalPathfinding ? EPathFindingMode::Hierarchical : EPathFindingMode::Regular;
	const FNavLocation NavMeshGoalLocation = ProjectPointToNavigation(GoalLocation);
	FPathFindingQuery Query(GetOwner(), GetNavData(), FromLocation, NavMeshGoalLocation.Location, QueryFilter);
	Query.NavDataFlags = NavDataFlags;

	FPathFindingResult Result = NavSys->FindPathSync(*MyNavAgent->GetNavAgentProperties(), Query, Mode);
	if (Result.IsSuccessful() || Result.IsPartial())
	{
		CurrentGoal = NavMeshGoalLocation.Location;
		SetPath(Result.Path);
		return true;
	}

	return false;
}

bool UNavigationComponent::GenerateSimplePath(const FVector& Destination)
{
	if (GetWorld() == NULL || GetOwner() == NULL || MyNavAgent == NULL)
	{
		return false;
	}
	
	TArray<FVector> PathPoints;
	PathPoints.Add(MyNavAgent->GetNavAgentLocation());
	PathPoints.Add(Destination);

	FNavPathSharedPtr SimplePath = MakeShareable(new FNavigationPath(PathPoints, NULL));
	
	CurrentGoal = Destination;
	SetPath(SimplePath);
	return true;
}

bool UNavigationComponent::GetPathPoint(uint32 PathVertIdx, FNavPathPoint& PathPoint) const
{
	if (Path.IsValid() && Path->IsValid() && (int32)PathVertIdx < Path->GetPathPoints().Num())
	{
		PathPoint = Path->GetPathPoints()[PathVertIdx];
		return true;
	}

	return false;
}

bool UNavigationComponent::HasValidPath() const
{ 
	return Path.IsValid() && Path->IsValid(); 
}

bool UNavigationComponent::FindSimplePathToActor(const AActor* NewGoalActor, float RepathDistance)
{
	bool bPathGenerationSucceeded = false;

	if (bIsPaused && NewGoalActor != NULL && GoalActor == NewGoalActor
		&& Path.IsValid() && Path->IsValid() && Path->GetNavigationDataUsed() == NULL)
	{
		RepathDistanceSq = FMath::Square(RepathDistance);
		OriginalGoalActorLocation = GetCurrentMoveGoal(NewGoalActor, GetOwner());

		bPathGenerationSucceeded = ResumePath();
	}
	else 
	{
		if (NewGoalActor != NULL)
		{
			const FVector NewGoalActorLocation = GetCurrentMoveGoal(NewGoalActor, GetOwner());
			bPathGenerationSucceeded = GenerateSimplePath(NewGoalActorLocation);

			if (bPathGenerationSucceeded)
			{
				GoalActor = NewGoalActor;
				RepathDistanceSq = FMath::Square(RepathDistance);
				OriginalGoalActorLocation = NewGoalActorLocation;
				bUseSimplePath = true;

				// if doing sync pathfinding enabling component's ticking needs to be done now
				//SetComponentTickEnabledAsync(true);
				Path->SetGoalActorObservation(*GoalActor, FMath::Sqrt(RepathDistanceSq));
			}
		}

		if (!bPathGenerationSucceeded)
		{
			UE_VLOG(GetOwner(), LogAINavigation, Warning, TEXT("Failed to generate simple path to %s"), *GetNameSafe(NewGoalActor));
		}
	}

	if (bPathGenerationSucceeded == false)
	{
		ResetTransientData();
	}

	return bPathGenerationSucceeded;
}

bool UNavigationComponent::FindPathToActor(const AActor* NewGoalActor, TSharedPtr<const FNavigationQueryFilter> QueryFilter, float RepathDistance)
{
	bool bPathGenerationSucceeded = false;

	if (bIsPaused && NewGoalActor != NULL && GoalActor == NewGoalActor 
		&& Path.IsValid() && Path->IsValid())
	{
		RepathDistanceSq = FMath::Square(RepathDistance);
		OriginalGoalActorLocation = GetCurrentMoveGoal(NewGoalActor, GetOwner());

		if (bDoAsyncPathfinding == true)
		{
			DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.Deferred resume path"),
				STAT_FSimpleDelegateGraphTask_DeferredResumePath,
				STATGROUP_TaskGraphTasks);

			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
				FSimpleDelegateGraphTask::FDelegate::CreateUObject(this, &UNavigationComponent::DeferredResumePath),
				GET_STATID(STAT_FSimpleDelegateGraphTask_DeferredResumePath), NULL, ENamedThreads::GameThread
			);
			bPathGenerationSucceeded = true;
		}
		else
		{
			bPathGenerationSucceeded = ResumePath();
		}
	}
	else
	{
		if (NewGoalActor != NULL)
		{
			if (QueryFilter.IsValid() == false)
			{
				QueryFilter = GetStoredQueryFilter();
			}

			const FVector NewGoalActorLocation = GetCurrentMoveGoal(NewGoalActor, GetOwner());
			if (bDoAsyncPathfinding == true)
			{
				bPathGenerationSucceeded = AsyncGeneratePathTo(NewGoalActorLocation, QueryFilter);
			}
			else
			{
				bPathGenerationSucceeded = GeneratePathTo(NewGoalActorLocation, QueryFilter);
			}

			if (bPathGenerationSucceeded)
			{
				// if there's a specific query filter store it
				StoredQueryFilter = QueryFilter;

				GoalActor = NewGoalActor;
				RepathDistanceSq = FMath::Square(RepathDistance);
				OriginalGoalActorLocation = NewGoalActorLocation;
				bUseSimplePath = false;

				if (bDoAsyncPathfinding == false)
				{
					// if doing sync pathfinding enabling component's ticking needs to be done now
					//SetComponentTickEnabledAsync(true);
					Path->SetGoalActorObservation(*GoalActor, FMath::Sqrt(RepathDistanceSq));
				}
			}
		}

		if (!bPathGenerationSucceeded)
		{
			UE_VLOG(GetOwner(), LogAINavigation, Log, TEXT("%s failed to generate path to %s"), *GetNameSafe(GetOwner()), *GetNameSafe(NewGoalActor));
		}
	}

	if (bPathGenerationSucceeded == false)
	{
		ResetTransientData();
	}

	return bPathGenerationSucceeded;
}

bool UNavigationComponent::FindSimplePathToLocation(const FVector& DestLocation)
{
	bool bPathGenerationSucceeded = false;

	if (bIsPaused && Path.IsValid() && Path->IsValid() && Path->GetNavigationDataUsed() == NULL
		&& FVector::DistSquared(CurrentGoal, DestLocation) < SMALL_NUMBER)
	{
		bPathGenerationSucceeded = ResumePath();
	}

	if (bPathGenerationSucceeded == false)
	{
		ResetTransientData();

		bPathGenerationSucceeded = GenerateSimplePath(DestLocation);
	}

	return bPathGenerationSucceeded;
}

bool UNavigationComponent::FindPathToLocation(const FVector& DestLocation, TSharedPtr<const FNavigationQueryFilter> QueryFilter)
{
	bool bPathGenerationSucceeded = false;

	if (bIsPaused == true && Path.IsValid() && Path->IsValid() 
		&& FVector::DistSquared(CurrentGoal, DestLocation) < SMALL_NUMBER)
	{
		// path to destination already generated. Use it.

		if (bDoAsyncPathfinding == true)
		{
			DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.Deferred resume path"),
				STAT_FSimpleDelegateGraphTask_DeferredResumePath,
				STATGROUP_TaskGraphTasks);

			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
				FSimpleDelegateGraphTask::FDelegate::CreateUObject(this, &UNavigationComponent::DeferredResumePath),
				GET_STATID(STAT_FSimpleDelegateGraphTask_DeferredResumePath), NULL, ENamedThreads::GameThread
			);
			bPathGenerationSucceeded = true;
		}
		else
		{
			bPathGenerationSucceeded = ResumePath();
		}
	}
	
	if (bPathGenerationSucceeded == false)
	{
		ResetTransientData();

		if (QueryFilter.IsValid() == false)
		{
			QueryFilter = GetStoredQueryFilter();
		}

		if (bDoAsyncPathfinding == true)
		{
			bPathGenerationSucceeded = AsyncGeneratePathTo(DestLocation, QueryFilter);
		}
		else
		{
			bPathGenerationSucceeded = GeneratePathTo(DestLocation, QueryFilter);
		}

		if (bPathGenerationSucceeded)
		{
			// if there's a specific query filter store it
			StoredQueryFilter = QueryFilter;
		}
	}

	return bPathGenerationSucceeded;
}

void UNavigationComponent::SetPath(const FNavPathSharedPtr& ResultPath)
{
	check(ResultPath.Get() != NULL);
	check(MyNavAgent);

	Path = ResultPath;
	Path->AddObserver(PathObserverDelegate);
	AActor* Owner = GetOwner();
	check(Owner);
	Path->SetSourceActor(*Owner);
	bIsPathPartial = Path->IsPartial();
}

void UNavigationComponent::ResetTransientData()
{
	GoalActor = NULL;
	RepathDistanceSq = 0.f;
	bIsPathPartial = false;
	bIsPaused = false;
	bUseSimplePath = false;
	Path = NULL;
	StoredQueryFilter = NULL;

	// stop ticking - this component should tick only when following an actor
	SetComponentTickEnabledAsync(false);
}

void UNavigationComponent::NotifyPathUpdate()
{
#if ENABLE_VISUAL_LOG
	if (Path.IsValid())
	{
		UE_VLOG(GetOwner(), LogAINavigation, Log, TEXT("NotifyPathUpdate %s"), *Path->GetDescription());
	}
	else
	{
		UE_VLOG(GetOwner(), LogAINavigation, Log, TEXT("NotifyPathUpdate: path MISSING!"));
	}
#endif

	if (MyPathObserver != NULL)
	{
		MyPathObserver->OnPathUpdated(this);
	}

	if (PathFollowComp && bNotifyPathFollowing && PathFollowComp->GetMoveGoal() == GoalActor)
	{
		PathFollowComp->UpdateMove(Path);
	}

	if (!Path.IsValid())
	{
		ResetTransientData();
	}
	else if (!Path->IsValid())
	{
		UE_VLOG(GetOwner(), LogAINavigation, Warning
			, TEXT("NotifyPathUpdate fetched invalid NavPath!  NavPath has %d points, is%sready, and is%sup-to-date")
			, Path->GetPathPoints().Num(), Path->IsReady() ? TEXT(" ") : TEXT(" NOT "), Path->IsUpToDate() ? TEXT(" ") : TEXT(" NOT "));

		ResetTransientData();
	}
}

void UNavigationComponent::DeferredRepathToGoal()
{
	check(ControllerOwner);

	// check if repath is allowed right now
	const bool bShouldPostponeRepath = ControllerOwner->ShouldPostponePathUpdates();
	if (bShouldPostponeRepath)
	{
		GetWorld()->GetTimerManager().SetTimer(this, &UNavigationComponent::DeferredRepathToGoal, 0.1f, false);
		return;
	}

	GoalActor = RepathData.GoalActor.Get();
	CurrentGoal = RepathData.GoalLocation;
	StoredQueryFilter = RepathData.StoredQueryFilter;
	bUseSimplePath = RepathData.bSimplePath;
	FMemory::MemZero<FDeferredRepath>(RepathData);

	const FVector GoalLocation = GoalActor ? GetCurrentMoveGoal(GoalActor, GetOwner()) : CurrentGoal;
	if (RePathTo(GoalLocation, StoredQueryFilter))
	{
		OriginalGoalActorLocation = GoalLocation;
	}
	else
	{
		FAIMessage::Send(ControllerOwner, FAIMessage(UBrainComponent::AIMessage_RepathFailed, this));
	}

	bIsWaitingForRepath = false;
}

void UNavigationComponent::SetPathFollowingUpdates(bool bEnabled)
{
	bNotifyPathFollowing = bEnabled;
}

void UNavigationComponent::SetRepathWhenInvalid(bool bEnabled)
{
	bRepathWhenInvalid = bEnabled;
}

void UNavigationComponent::SetComponentTickEnabledAsync(bool bEnabled)
{
	/*bShoudTick = bEnabled;
	Super::SetComponentTickEnabledAsync(bEnabled);*/
}

void UNavigationComponent::OnNavAgentChanged()
{
	MyNavAgent = NULL;
	PickNavData();
}

ANavigationData* UNavigationComponent::PickNavData() const
{
	// reset to default
	MyNavData = NULL;

	if (MyNavAgent == NULL)
	{
		MyNavAgent = Cast<INavAgentInterface>( GetOuter() );
	}

	if (MyPathObserver == NULL)
	{
		MyPathObserver = Cast<INavPathObserverInterface>( GetOuter() );
	}

	if (GetWorld() != NULL && GetWorld()->GetNavigationSystem() != NULL && MyNavAgent != NULL)
	{
		const FNavAgentProperties* MoveProps = MyNavAgent->GetNavAgentProperties();

		// Make sure any previous registration for the event is cleared.
		GetWorld()->GetNavigationSystem()->OnNavDataRegisteredEvent.RemoveDynamic(this, &UNavigationComponent::OnNavDataRegistered);
		
		if (MoveProps != NULL)
		{
			// cache it
			MyNavData = GetWorld()->GetNavigationSystem()->GetNavDataForProps(*MoveProps);
		}

		if (MyNavData == NULL)
		{	// Failed to pick it, so register to try again.
			// note that if this happens a lot there's a bigger problem like AIs being spawned before navmesh is ready
			GetWorld()->GetNavigationSystem()->OnNavDataRegisteredEvent.AddDynamic(this, &UNavigationComponent::OnNavDataRegistered);
		}
	}

	CacheNavQueryExtent();

	return MyNavData;
}

void UNavigationComponent::CacheNavQueryExtent() const
{
	NavigationQueryExtent = MyNavData ? MyNavData->GetDefaultQueryExtent() : FVector(DEFAULT_NAV_QUERY_EXTENT_HORIZONTAL, DEFAULT_NAV_QUERY_EXTENT_HORIZONTAL, DEFAULT_NAV_QUERY_EXTENT_VERTICAL);
	if (MyNavAgent)
	{
		const FNavAgentProperties* AgentProperties = MyNavAgent->GetNavAgentProperties();
		check(AgentProperties);
		NavigationQueryExtent = FVector(FMath::Max(NavigationQueryExtent.X, AgentProperties->AgentRadius)
			, FMath::Max(NavigationQueryExtent.Y, AgentProperties->AgentRadius)
			, FMath::Max(NavigationQueryExtent.Z, AgentProperties->AgentHeight / 2));
	}
}

void UNavigationComponent::OnNavDataRegistered(ANavigationData* NavData)
{
	// re-pick regardless of whether we have MyNavData set already - the new one can be better!
	PickNavData();
}

void UNavigationComponent::DrawDebugCurrentPath(UCanvas* Canvas, FColor PathColor, const uint32 NextPathPointIndex)
{
	if (MyNavData == NULL)
	{
		return;
	}

	const bool bPersistent = Canvas == NULL;

	if (bPersistent)
	{
		FlushPersistentDebugLines( GetWorld() );
	}

	DrawDebugBox(GetWorld(), CurrentGoal, FVector(10.f), PathColor, bPersistent);
	if (Path.IsValid())
	{
		MyNavData->DrawDebugPath(Path.Get(), PathColor, Canvas, bPersistent, NextPathPointIndex);
	}
}

void UNavigationComponent::SetReceiveSmartLinkUpdates(bool bEnabled)
{
	bUpdateForSmartLinks = bEnabled;
}

void UNavigationComponent::OnCustomLinkBroadcast(UNavLinkCustomComponent* NearbyLink)
{
	// update only when agent is actually moving
	if (NearbyLink == NULL || !Path.IsValid() ||
		PathFollowComp == NULL || PathFollowComp->GetStatus() == EPathFollowingStatus::Idle)
	{
		return;
	}

	UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
	if (NavSys == NULL || MyNavAgent == NULL || GetNavData() == NULL)
	{
		return;
	}

	const bool bHasLink = Path->ContainsCustomLink(NearbyLink->GetLinkId());
	const bool bIsEnabled = NearbyLink->IsEnabled();
	if (bIsEnabled == bHasLink)
	{
		// enabled link, path already use it - or - disabled link and path it's not using it
		return;
	}

	const EPathFindingMode::Type Mode = bDoHierarchicalPathfinding ? EPathFindingMode::Hierarchical : EPathFindingMode::Regular;
	const FVector GoalLocation = Path->GetPathPoints().Last().Location;
	FPathFindingQuery Query(GetOwner(), GetNavData(), MyNavAgent->GetNavAgentLocation(), GoalLocation, StoredQueryFilter);
	Query.NavDataFlags = NavDataFlags;

	FPathFindingResult Result = NavSys->FindPathSync(*MyNavAgent->GetNavAgentProperties(), Query, Mode);
	if (Result.IsSuccessful() || Result.IsPartial())
	{
		const bool bNewHasLink = Result.Path->ContainsCustomLink(NearbyLink->GetLinkId());

		if ((bIsEnabled && !bHasLink && bNewHasLink) ||
			(!bIsEnabled && bHasLink && !bNewHasLink))
		{			
			SetPath(Result.Path);
			NotifyPathUpdate();
		}
	}
}

void UNavigationComponent::SwapCurrentMoveGoal(const AActor* NewGoalActor)
{
	if (NewGoalActor != NULL && GoalActor != NULL && GoalActor != NewGoalActor && Path.IsValid() && Path->IsValid() &&
		PathFollowComp != NULL && PathFollowComp->GetStatus() != EPathFollowingStatus::Idle )
	{
		UE_VLOG(GetOwner(), LogAINavigation, Log, TEXT("GoalActor is going to be swapped for '%s'"), *NewGoalActor->GetHumanReadableName());
		GoalActor = NewGoalActor;
		PathFollowComp->SetDestinationActor( NewGoalActor );
	}
}

#if ENABLE_VISUAL_LOG
//----------------------------------------------------------------------//
// debug
//----------------------------------------------------------------------//
void UNavigationComponent::DescribeSelfToVisLog(FVisualLogEntry* Snapshot) const
{
	// path will be drawn by path following
}

#endif // ENABLE_VISUAL_LOG
