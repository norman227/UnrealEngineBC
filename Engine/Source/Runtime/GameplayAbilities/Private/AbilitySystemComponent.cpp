// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "AbilitySystemComponent.h"
#include "GameplayCueInterface.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "GameplayCueManager.h"

#include "Net/UnrealNetwork.h"
#include "Engine/ActorChannel.h"
#include "MessageLog.h"
#include "UObjectToken.h"
#include "MapErrors.h"

DEFINE_LOG_CATEGORY(LogAbilitySystemComponent);

#define LOCTEXT_NAMESPACE "AbilitySystemComponent"


int32 DebugGameplayCues = 0;
static FAutoConsoleVariableRef CVarDebugGameplayCues(
	TEXT("AbilitySystem.DebugGameplayCues"),
	DebugGameplayCues,
	TEXT("Enables Debugging for GameplayCue events"),
	ECVF_Default
	);

/** Enable to log out all render state create, destroy and updatetransform events */
#define LOG_RENDER_STATE 0

UAbilitySystemComponent::UAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, GameplayTagCountContainer(EGameplayTagMatchType::IncludeParentTags)
{
	bWantsInitializeComponent = true;

	PrimaryComponentTick.TickGroup = TG_DuringPhysics;
	PrimaryComponentTick.bStartWithTickEnabled = true; // FIXME! Just temp until timer manager figured out
	PrimaryComponentTick.bCanEverTick = true;
	
	ActiveGameplayCues.Owner = this;

	bReplicates = true;

	UserAbilityActivationInhibited = false;
}

UAbilitySystemComponent::~UAbilitySystemComponent()
{
	ActiveGameplayEffects.PreDestroy();
}

const UAttributeSet* UAbilitySystemComponent::InitStats(TSubclassOf<class UAttributeSet> Attributes, const UDataTable* DataTable)
{
	const UAttributeSet* AttributeObj = NULL;
	if (Attributes)
	{
		AttributeObj = GetOrCreateAttributeSubobject(Attributes);
		if (AttributeObj && DataTable)
		{
			// This const_cast is OK - this is one of the few places we want to directly modify our AttributeSet properties rather
			// than go through a gameplay effect
			const_cast<UAttributeSet*>(AttributeObj)->InitFromMetaDataTable(DataTable);
		}
	}
	return AttributeObj;
}

void UAbilitySystemComponent::K2_InitStats(TSubclassOf<class UAttributeSet> Attributes, const UDataTable* DataTable)
{
	InitStats(Attributes, DataTable);
}

const UAttributeSet* UAbilitySystemComponent::GetOrCreateAttributeSubobject(TSubclassOf<UAttributeSet> AttributeClass)
{
	AActor *OwningActor = GetOwner();
	const UAttributeSet *MyAttributes  = NULL;
	if (OwningActor && AttributeClass)
	{
		MyAttributes = GetAttributeSubobject(AttributeClass);
		if (!MyAttributes)
		{
			UAttributeSet *Attributes = ConstructObject<UAttributeSet>(AttributeClass, OwningActor);
			SpawnedAttributes.AddUnique(Attributes);
			MyAttributes = Attributes;
		}
	}

	return MyAttributes;
}

const UAttributeSet* UAbilitySystemComponent::GetAttributeSubobjectChecked(const TSubclassOf<UAttributeSet> AttributeClass) const
{
	const UAttributeSet *Set = GetAttributeSubobject(AttributeClass);
	check(Set);
	return Set;
}

const UAttributeSet* UAbilitySystemComponent::GetAttributeSubobject(const TSubclassOf<UAttributeSet> AttributeClass) const
{
	for (const UAttributeSet* Set : SpawnedAttributes)
	{
		if (Set && Set->IsA(AttributeClass))
		{
			return Set;
		}
	}
	return NULL;
}

bool UAbilitySystemComponent::HasAttributeSetForAttribute(FGameplayAttribute Attribute) const
{
	return (GetAttributeSubobject(Attribute.GetAttributeSetClass()) != nullptr);
}

void UAbilitySystemComponent::OnRegister()
{
	Super::OnRegister();

	// Init starting data
	for (int32 i=0; i < DefaultStartingData.Num(); ++i)
	{
		if (DefaultStartingData[i].Attributes && DefaultStartingData[i].DefaultStartingTable)
		{
			UAttributeSet* Attributes = const_cast<UAttributeSet*>(GetOrCreateAttributeSubobject(DefaultStartingData[i].Attributes));
			Attributes->InitFromMetaDataTable(DefaultStartingData[i].DefaultStartingTable);
		}
	}

	ActiveGameplayEffects.RegisterWithOwner(this);
}

// ---------------------------------------------------------

bool UAbilitySystemComponent::IsOwnerActorAuthoritative() const
{
	return !IsNetSimulating();
}

bool UAbilitySystemComponent::HasNetworkAuthorityToApplyGameplayEffect(FPredictionKey PredictionKey) const
{
	return (IsOwnerActorAuthoritative() || PredictionKey.IsValidForMorePrediction());
}

void UAbilitySystemComponent::SetNumericAttribute(const FGameplayAttribute &Attribute, float NewFloatValue)
{
	const UAttributeSet* AttributeSet = GetAttributeSubobjectChecked(Attribute.GetAttributeSetClass());
	Attribute.SetNumericValueChecked(NewFloatValue, const_cast<UAttributeSet*>(AttributeSet));
}

float UAbilitySystemComponent::GetNumericAttribute(const FGameplayAttribute &Attribute)
{
	const UAttributeSet* AttributeSet = GetAttributeSubobjectChecked(Attribute.GetAttributeSetClass());
	return Attribute.GetNumericValueChecked(AttributeSet);
}

FGameplayEffectSpecHandle UAbilitySystemComponent::GetOutgoingSpec(UGameplayEffect* GameplayEffect, float Level) const
{
	SCOPE_CYCLE_COUNTER(STAT_GetOutgoingSpec);
	// Fixme: we should build a map and cache these off. We can invalidate the map when an OutgoingGE modifier is applied or removed from us.

	// By default use the owner and avatar as the instigator and causer
	FGameplayEffectSpec* NewSpec = new FGameplayEffectSpec(GameplayEffect, GetEffectContext(), Level);
	return FGameplayEffectSpecHandle(NewSpec);
}

FGameplayEffectContextHandle UAbilitySystemComponent::GetEffectContext() const
{
	FGameplayEffectContextHandle Context = FGameplayEffectContextHandle(UAbilitySystemGlobals::Get().AllocGameplayEffectContext());
	// By default use the owner and avatar as the instigator and causer
	Context.AddInstigator(AbilityActorInfo->OwnerActor.Get(), AbilityActorInfo->AvatarActor.Get());
	return Context;
}

/** This is a helper function used in automated testing, I'm not sure how useful it will be to gamecode or blueprints */
FActiveGameplayEffectHandle UAbilitySystemComponent::ApplyGameplayEffectToTarget(UGameplayEffect *GameplayEffect, UAbilitySystemComponent *Target, float Level, FGameplayEffectContextHandle Context, FPredictionKey PredictionKey)
{
	check(GameplayEffect);
	if (HasNetworkAuthorityToApplyGameplayEffect(PredictionKey))
	{
		if (!Context.IsValid())
		{
			Context = GetEffectContext();
		}

		FGameplayEffectSpec	Spec(GameplayEffect, Context, Level);
		return ApplyGameplayEffectSpecToTarget(Spec, Target, PredictionKey);
	}

	return FActiveGameplayEffectHandle();
}

/** Helper function since we can't have default/optional values for FModifierQualifier in K2 function */
FActiveGameplayEffectHandle UAbilitySystemComponent::K2_ApplyGameplayEffectToSelf(const UGameplayEffect *GameplayEffect, float Level, FGameplayEffectContextHandle EffectContext)
{
	return ApplyGameplayEffectToSelf(GameplayEffect, Level, EffectContext);
}

/** This is a helper function - it seems like this will be useful as a blueprint interface at the least, but Level parameter may need to be expanded */
FActiveGameplayEffectHandle UAbilitySystemComponent::ApplyGameplayEffectToSelf(const UGameplayEffect *GameplayEffect, float Level, const FGameplayEffectContextHandle& EffectContext, FPredictionKey PredictionKey)
{
	if (GameplayEffect == nullptr)
	{
		ABILITY_LOG(Error, TEXT("UAbilitySystemComponent::ApplyGameplayEffectToSelf called by Instigator %s with a null GameplayEffect."), *EffectContext.ToString());
		return FActiveGameplayEffectHandle();
	}

	if (HasNetworkAuthorityToApplyGameplayEffect(PredictionKey))
	{
		FGameplayEffectSpec	Spec(GameplayEffect, EffectContext, Level);
		return ApplyGameplayEffectSpecToSelf(Spec, PredictionKey);
	}

	return FActiveGameplayEffectHandle();
}

FOnActiveGameplayEffectRemoved* UAbilitySystemComponent::OnGameplayEffectRemovedDelegate(FActiveGameplayEffectHandle Handle)
{
	FActiveGameplayEffect* ActiveEffect = ActiveGameplayEffects.GetActiveGameplayEffect(Handle);
	if (ActiveEffect)
	{
		return &ActiveEffect->OnRemovedDelegate;
	}

	return nullptr;
}

int32 UAbilitySystemComponent::GetNumActiveGameplayEffect() const
{
	return ActiveGameplayEffects.GetNumGameplayEffects();
}

bool UAbilitySystemComponent::IsGameplayEffectActive(FActiveGameplayEffectHandle InHandle) const
{
	return ActiveGameplayEffects.IsGameplayEffectActive(InHandle);
}

void UAbilitySystemComponent::CaptureAttributeForGameplayEffect(OUT FGameplayEffectAttributeCaptureSpec& OutCaptureSpec)
{
	// Verify the capture is happening on an attribute the component actually has a set for; if not, can't capture the value
	const FGameplayAttribute& AttributeToCapture = OutCaptureSpec.BackingDefinition.AttributeToCapture;
	if (AttributeToCapture.IsValid() && GetAttributeSubobject(AttributeToCapture.GetAttributeSetClass()))
	{
		ActiveGameplayEffects.CaptureAttributeForGameplayEffect(OutCaptureSpec);
	}
}

FOnGameplayEffectTagCountChanged& UAbilitySystemComponent::RegisterGameplayTagEvent(FGameplayTag Tag)
{
	return GameplayTagCountContainer.GameplayTagEventMap.FindOrAdd(Tag);
}

FOnGameplayEffectTagCountChanged& UAbilitySystemComponent::RegisterGenericGameplayTagEvent()
{
	return GameplayTagCountContainer.OnAnyTagChangeDelegate;
}

FOnGameplayAttributeChange& UAbilitySystemComponent::RegisterGameplayAttributeEvent(FGameplayAttribute Attribute)
{
	return ActiveGameplayEffects.RegisterGameplayAttributeEvent(Attribute);
}

// ------------------------------------------------------------------------

void UAbilitySystemComponent::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	SCOPE_CYCLE_COUNTER(STAT_GameplayEffectsGetOwnedTags);
	for (auto It = GameplayTagCountContainer.GameplayTagCountMap.CreateConstIterator(); It; ++It)
	{
		if (It.Value() > 0)
		{
			TagContainer.AddTagFast(It.Key());
		}
	}
}

bool UAbilitySystemComponent::HasMatchingGameplayTag(FGameplayTag TagToCheck) const
{
	return GameplayTagCountContainer.HasMatchingGameplayTag(TagToCheck, EGameplayTagMatchType::Explicit);
}

bool UAbilitySystemComponent::HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer, bool bCountEmptyAsMatch) const
{
	SCOPE_CYCLE_COUNTER(STAT_GameplayEffectsHasAllTags);
	return GameplayTagCountContainer.HasAllMatchingGameplayTags(TagContainer, EGameplayTagMatchType::Explicit, bCountEmptyAsMatch);
}

bool UAbilitySystemComponent::HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer, bool bCountEmptyAsMatch) const
{
	SCOPE_CYCLE_COUNTER(STAT_GameplayEffectsHasAnyTag);
	return GameplayTagCountContainer.HasAnyMatchingGameplayTags(TagContainer, EGameplayTagMatchType::Explicit, bCountEmptyAsMatch);
}

void UAbilitySystemComponent::AddLooseGameplayTag(const FGameplayTag& GameplayTag, int32 Count)
{
	UpdateTagMap(GameplayTag, Count);
}

void UAbilitySystemComponent::AddLooseGameplayTags(const FGameplayTagContainer& GameplayTags, int32 Count)
{
	UpdateTagMap(GameplayTags, Count);
}

void UAbilitySystemComponent::RemoveLooseGameplayTag(const FGameplayTag& GameplayTag, int32 Count)
{
	UpdateTagMap(GameplayTag, Count);
}

void UAbilitySystemComponent::RemoveLooseGameplayTags(const FGameplayTagContainer& GameplayTags, int32 Count)
{
	UpdateTagMap(GameplayTags, Count);
}

// These are functionally redundant but are called by GEs and GameplayCues that add tags that are not 'loose' (but are handled the same way in practice)

void UAbilitySystemComponent::UpdateTagMap(const FGameplayTag& BaseTag, int32 CountDelta)
{
	GameplayTagCountContainer.UpdateTagMap(BaseTag, CountDelta);
}

void UAbilitySystemComponent::UpdateTagMap(const FGameplayTagContainer& Container, int32 CountDelta)
{
	GameplayTagCountContainer.UpdateTagMap(Container, CountDelta);
}

// ------------------------------------------------------------------------

FActiveGameplayEffectHandle UAbilitySystemComponent::ApplyGameplayEffectSpecToTarget(OUT FGameplayEffectSpec &Spec, UAbilitySystemComponent *Target, FPredictionKey PredictionKey)
{
	if (HasNetworkAuthorityToApplyGameplayEffect(PredictionKey))
	{
		// Apply outgoing Effects to the Spec.
		// Outgoing immunity may stop the outgoing effect from being applied to the target
		return Target->ApplyGameplayEffectSpecToSelf(Spec, PredictionKey);
	}

	return FActiveGameplayEffectHandle();
}

FActiveGameplayEffectHandle UAbilitySystemComponent::ApplyGameplayEffectSpecToSelf(OUT FGameplayEffectSpec &Spec, FPredictionKey PredictionKey)
{
	// Temp, only non instant, non periodic GEs can be predictive
	// Effects with other effects may be a mix so go with non-predictive
	check((PredictionKey.IsValidKey() == false) || (Spec.GetPeriod() == UGameplayEffect::NO_PERIOD));

	// Check Tag requirements
	if (!HasNetworkAuthorityToApplyGameplayEffect(PredictionKey))
	{
		return FActiveGameplayEffectHandle();
	}

	// Check AttributeSet requirements: do we have everything this GameplayEffectSpec expects?
	// We may want to cache this off in some way to make the runtime check quicker.
	// We also need to handle things in the execution list
	for (const FModifierSpec& Mod : Spec.Modifiers)
	{
		if (HasAttributeSetForAttribute(Mod.Info.Attribute) == false)
		{
			return FActiveGameplayEffectHandle();
		}
	}

	// check if the effect being applied actually succeeds
	float ChanceToApply = Spec.GetChanceToApplyToTarget();
	if ((ChanceToApply < 1.f - SMALL_NUMBER) && (FMath::FRand() > ChanceToApply))
	{
		return FActiveGameplayEffectHandle();
	}

	// Get MyTags.
	//	We may want to cache off a GameplayTagContainer instead of rebuilding it everytime.
	//	But this will also be where we need to merge in context tags? (Headshot, executing ability, etc?)
	//	Or do we push these tags into (our copy of the spec)?

	FGameplayTagContainer MyTags;
	GetOwnedGameplayTags(MyTags);

	if (Spec.Def->ApplicationTagRequirements.RequirementsMet(MyTags) == false)
	{
		return FActiveGameplayEffectHandle();
	}
	

	// Clients should treat predicted instant effects as if they have infinite duration. The effects will be cleaned up later.
	bool bTreatAsInfiniteDuration = GetOwnerRole() != ROLE_Authority && PredictionKey.IsValidKey() && Spec.GetDuration() == UGameplayEffect::INSTANT_APPLICATION;

	// Make sure we create our copy of the spec in the right place
	FActiveGameplayEffectHandle	MyHandle;
	bool bInvokeGameplayCueApplied = UGameplayEffect::INSTANT_APPLICATION != Spec.GetDuration(); // Cache this now before possibly modifying predictive instant effect to infinite duration effect.

	FGameplayEffectSpec* OurCopyOfSpec = NULL;
	TSharedPtr<FGameplayEffectSpec> StackSpec;
	float Duration = bTreatAsInfiniteDuration ? UGameplayEffect::INFINITE_DURATION : Spec.GetDuration();
	{
		if (Duration != UGameplayEffect::INSTANT_APPLICATION)
		{
			// recalculating stacking needs to come before creating the new effect
			if (Spec.GetStackingType() != EGameplayEffectStackingPolicy::Unlimited)
			{
				ActiveGameplayEffects.StacksNeedToRecalculate();
			}
			FActiveGameplayEffect& NewActiveEffect = ActiveGameplayEffects.CreateNewActiveGameplayEffect(Spec, PredictionKey);
			MyHandle = NewActiveEffect.Handle;
			OurCopyOfSpec = &NewActiveEffect.Spec;
		}
		
		if (!OurCopyOfSpec)
		{
			StackSpec = TSharedPtr<FGameplayEffectSpec>(new FGameplayEffectSpec(Spec));
			OurCopyOfSpec = StackSpec.Get();
		}

		// GE_REMOVE: Pretty sure this completely goes away? 

		// Do a 1st order copy of the spec so that we can modify it
		// (the one passed in is owned by the caller, we can't apply our incoming GEs to it)
		// Note that at this point the spec has a bunch of modifiers. Those modifiers may
		// have other modifiers. THOSE modifiers may or may not be copies of whatever.
		//
		// In theory, we don't modify 2nd order modifiers after they are 'attached'
		// Long complex chains can be created but we never say 'Modify a GE that is modding another GE'
		// 
		// 
		// OurCopyOfSpec->MakeUnique();

		// if necessary add a modifier to OurCopyOfSpec to force it to have an infinite duration
		if (bTreatAsInfiniteDuration)
		{			
			// This should just be a straight set of the duration float now
		}
	}

	// Capture Our attributes (this may snapshot or link to our AttributeAggregators)
	OurCopyOfSpec->CapturedRelevantAttributes.CaptureAttributes(this, EGameplayEffectAttributeCaptureSource::Target);
	
	// We still probably want to apply tags and stuff even if instant?
	if (bInvokeGameplayCueApplied)
	{
		// We both added and activated the GameplayCue here.
		// On the client, who will invoke the gameplay cue from an OnRep, he will need to look at the StartTime to determine
		// if the Cue was actually added+activated or just added (due to relevancy)

		// Fixme: what if we wanted to scale Cue magnitude based on damage? E.g, scale an cue effect when the GE is buffed?
		InvokeGameplayCueEvent(*OurCopyOfSpec, EGameplayCueEvent::OnActive);
		InvokeGameplayCueEvent(*OurCopyOfSpec, EGameplayCueEvent::WhileActive);
	}
	
	// Execute the GE at least once (if instant, this will execute once and be done. If persistent, it was added to ActiveGameplayEffects above)
	
	// Execute if this is an instant application effect
	if (Duration == UGameplayEffect::INSTANT_APPLICATION)
	{
		ExecuteGameplayEffect(*OurCopyOfSpec, PredictionKey);
	}
	else if (bTreatAsInfiniteDuration)
	{
		// This is an instant application but we are treating it as an infinite duration for prediction. We should still predict the execute GameplayCUE.
		// (in non predictive case, this will happen inside ::ExecuteGameplayEffect)
		InvokeGameplayCueEvent(*OurCopyOfSpec, EGameplayCueEvent::Executed);
	}


	if (Spec.GetPeriod() != UGameplayEffect::NO_PERIOD && Spec.TargetEffectSpecs.Num() > 0)
	{
		ABILITY_LOG(Warning, TEXT("%s is periodic but also applies GameplayEffects to its target. GameplayEffects will only be applied once, not every period."), *Spec.Def->GetPathName());
	}
	// todo: this is ignoring the returned handles, should we put them into a TArray and return all of the handles?
	for (const TSharedRef<FGameplayEffectSpec> TargetSpec : Spec.TargetEffectSpecs)
	{
		ApplyGameplayEffectSpecToSelf(TargetSpec.Get(), PredictionKey);
	}

	return MyHandle;
}

void UAbilitySystemComponent::ExecutePeriodicEffect(FActiveGameplayEffectHandle	Handle)
{
	ActiveGameplayEffects.ExecutePeriodicGameplayEffect(Handle);
}

void UAbilitySystemComponent::ExecuteGameplayEffect(FGameplayEffectSpec &Spec, FPredictionKey PredictionKey)
{
	// Should only ever execute effects that are instant application or periodic application
	// Effects with no period and that aren't instant application should never be executed
	check( (Spec.GetDuration() == UGameplayEffect::INSTANT_APPLICATION || Spec.GetPeriod() != UGameplayEffect::NO_PERIOD) );
	
	ActiveGameplayEffects.ExecuteActiveEffectsFrom(Spec, PredictionKey);
}

void UAbilitySystemComponent::CheckDurationExpired(FActiveGameplayEffectHandle Handle)
{
	ActiveGameplayEffects.CheckDuration(Handle);
}

bool UAbilitySystemComponent::RemoveActiveGameplayEffect(FActiveGameplayEffectHandle Handle)
{
	return ActiveGameplayEffects.RemoveActiveGameplayEffect(Handle);
}

float UAbilitySystemComponent::GetGameplayEffectDuration(FActiveGameplayEffectHandle Handle) const
{
	return ActiveGameplayEffects.GetGameplayEffectDuration(Handle);
}

float UAbilitySystemComponent::GetGameplayEffectMagnitude(FActiveGameplayEffectHandle Handle, FGameplayAttribute Attribute) const
{
	return ActiveGameplayEffects.GetGameplayEffectMagnitude(Handle, Attribute);
}

void UAbilitySystemComponent::InvokeGameplayCueEvent(const FGameplayEffectSpec &Spec, EGameplayCueEvent::Type EventType)
{
	AActor* ActorAvatar = AbilityActorInfo->AvatarActor.Get();
	if (!Spec.Def)
	{
		ABILITY_LOG(Warning, TEXT("InvokeGameplayCueEvent Actor %s that has no gameplay effect!"), ActorAvatar ? *ActorAvatar->GetName() : TEXT("NULL"));
		return;
	}

	if (DebugGameplayCues)
	{
		ABILITY_LOG(Warning, TEXT("InvokeGameplayCueEvent: %s"), *Spec.ToSimpleString());
	}
	
	float ExecuteLevel = Spec.GetLevel();

	FGameplayCueParameters CueParameters;
	CueParameters.EffectContext = Spec.EffectContext;

	for (FGameplayEffectCue CueInfo : Spec.Def->GameplayCues)
	{
		if (CueInfo.MagnitudeAttribute.IsValid())
		{
			if (const FGameplayEffectModifiedAttribute* ModifiedAttribute = Spec.GetModifiedAttribute(CueInfo.MagnitudeAttribute))
			{
				CueParameters.RawMagnitude = ModifiedAttribute->TotalMagnitude;
			}
			else
			{
				CueParameters.RawMagnitude = 0.0f;
			}
		}
		else
		{
			CueParameters.RawMagnitude = 0.0f;
		}

		CueParameters.NormalizedMagnitude = CueInfo.NormalizeLevel(ExecuteLevel);

		UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCues(ActorAvatar, CueInfo.GameplayCueTags, EventType, CueParameters);

		if (DebugGameplayCues && Spec.EffectContext.GetHitResult())
		{
			DrawDebugSphere(GetWorld(), Spec.EffectContext.GetHitResult()->Location, 30.f, 32, FColor(255, 0, 0), true, 30.f);
			ABILITY_LOG(Warning, TEXT("   %s"), *CueInfo.GameplayCueTags.ToString());
		}
	}
}

void UAbilitySystemComponent::InvokeGameplayCueEvent(const FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, FGameplayEffectContextHandle EffectContext)
{
	AActor* ActorAvatar = AbilityActorInfo->AvatarActor.Get();
	AActor* ActorOwner = AbilityActorInfo->OwnerActor.Get();
	IGameplayCueInterface* GameplayCueInterface = Cast<IGameplayCueInterface>(ActorAvatar);
	if (!GameplayCueInterface)
	{
		return;
	}

	FGameplayCueParameters CueParameters;

	if (EffectContext.IsValid())
	{
		CueParameters.EffectContext = EffectContext;
	}
	else
	{
		CueParameters.EffectContext.AddInstigator(ActorOwner, ActorAvatar); // By default use the owner and avatar as the instigator and causer
	}

	CueParameters.NormalizedMagnitude = 1.f;
	CueParameters.RawMagnitude = 0.f;

	UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCues(ActorAvatar, GameplayCueTag, EventType, CueParameters);
}

void UAbilitySystemComponent::ExecuteGameplayCue(const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayEffectContextHandle EffectContext)
{
	if (IsOwnerActorAuthoritative())
	{
		NetMulticast_InvokeGameplayCueExecuted(GameplayCueTag, PredictionKey, EffectContext);
	}
	else if (PredictionKey.IsValidKey())
	{
		InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::Executed, EffectContext);
	}
}

void UAbilitySystemComponent::AddGameplayCue(const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayEffectContextHandle EffectContext)
{
	if (IsOwnerActorAuthoritative())
	{
		ActiveGameplayCues.AddCue(GameplayCueTag);
		NetMulticast_InvokeGameplayCueAdded(GameplayCueTag, PredictionKey, EffectContext);
	}
	else if (PredictionKey.IsValidKey())
	{
		// Allow for predictive gameplaycue events? Needs more thought
		InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::OnActive, EffectContext);
		InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::WhileActive);
	}
}

void UAbilitySystemComponent::RemoveGameplayCue(const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey)
{
	if (IsOwnerActorAuthoritative())
	{
		ActiveGameplayCues.RemoveCue(GameplayCueTag);
		NetMulticast_InvokeGameplayCueRemoved(GameplayCueTag, PredictionKey);
	}
	else if (PredictionKey.IsValidKey())
	{
		InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::Removed);
	}
}

void UAbilitySystemComponent::NetMulticast_InvokeGameplayCueExecuted_FromSpec_Implementation(const FGameplayEffectSpec Spec, FPredictionKey PredictionKey)
{
	if (IsOwnerActorAuthoritative() || PredictionKey.IsValidKey() == false)
	{
		InvokeGameplayCueEvent(Spec, EGameplayCueEvent::Executed);
	}
}

void UAbilitySystemComponent::NetMulticast_InvokeGameplayCueExecuted_Implementation(const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayEffectContextHandle EffectContext)
{
	if (IsOwnerActorAuthoritative() || PredictionKey.IsValidKey() == false)
	{
		InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::Executed, EffectContext);
	}
}

void UAbilitySystemComponent::NetMulticast_InvokeGameplayCueAdded_Implementation(const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayEffectContextHandle EffectContext)
{
	if (IsOwnerActorAuthoritative() || PredictionKey.IsValidKey() == false)
	{
		InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::OnActive, EffectContext);
	}
}

void UAbilitySystemComponent::NetMulticast_InvokeGameplayCueRemoved_Implementation(const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey)
{
	if (IsOwnerActorAuthoritative() || PredictionKey.IsValidKey() == false)
	{
		InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::Removed);
	}
}

bool UAbilitySystemComponent::IsGameplayCueActive(const FGameplayTag GameplayCueTag) const
{
	return HasMatchingGameplayTag(GameplayCueTag);
}

// ----------------------------------------------------------------------------------------

void UAbilitySystemComponent::SetBaseAttributeValueFromReplication(float NewValue, FGameplayAttribute Attribute)
{
	ActiveGameplayEffects.SetBaseAttributeValueFromReplication(Attribute, NewValue);
}

bool UAbilitySystemComponent::CanApplyAttributeModifiers(const UGameplayEffect *GameplayEffect, float Level, const FGameplayEffectContextHandle& EffectContext)
{
	return ActiveGameplayEffects.CanApplyAttributeModifiers(GameplayEffect, Level, EffectContext);
}

TArray<float> UAbilitySystemComponent::GetActiveEffectsTimeRemaining(const FActiveGameplayEffectQuery Query) const
{
	return ActiveGameplayEffects.GetActiveEffectsTimeRemaining(Query);
}

TArray<float> UAbilitySystemComponent::GetActiveEffectsDuration(const FActiveGameplayEffectQuery Query) const
{
	return ActiveGameplayEffects.GetActiveEffectsDuration(Query);
}

void UAbilitySystemComponent::RemoveActiveEffects(const FActiveGameplayEffectQuery Query)
{
	return ActiveGameplayEffects.RemoveActiveEffects(Query);
}

void UAbilitySystemComponent::OnRestackGameplayEffects()
{
	ActiveGameplayEffects.RecalculateStacking();
}

// ---------------------------------------------------------------------------------------

void UAbilitySystemComponent::TaskStarted(UAbilityTask* NewTask)
{
	if (NewTask->bTickingTask)
	{
		// If this is our first ticking task, set this component as active so it begins ticking
		if (TickingTasks.Num() == 0)
		{
			UpdateShouldTick();
		}
		check(TickingTasks.Contains(NewTask) == false);
		TickingTasks.Add(NewTask);
	}
	if (NewTask->bSimulatedTask)
	{
		check(SimulatedTasks.Contains(NewTask) == false);
		SimulatedTasks.Add(NewTask);
	}
}

void UAbilitySystemComponent::TaskEnded(UAbilityTask* Task)
{
	if (Task->bTickingTask)
	{
		// If we are removing our last ticking task, set this component as inactive so it stops ticking
		TickingTasks.RemoveSingleSwap(Task);
		if (TickingTasks.Num() == 0)
		{
			UpdateShouldTick();
		}
	}

	if (Task->bSimulatedTask)
	{
		SimulatedTasks.RemoveSingleSwap(Task);
	}
}

// ---------------------------------------------------------------------------------------

void UAbilitySystemComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	// Intentionally not calling super: We do not want to replicate bActive which controls ticking. We sometimes need to tick on client predictively.
	

	DOREPLIFETIME(UAbilitySystemComponent, SpawnedAttributes);
	DOREPLIFETIME(UAbilitySystemComponent, ActiveGameplayEffects);
	DOREPLIFETIME(UAbilitySystemComponent, ActiveGameplayCues);
	
	DOREPLIFETIME_CONDITION(UAbilitySystemComponent, ActivatableAbilities, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UAbilitySystemComponent, BlockedAbilityBindings, COND_OwnerOnly)

	DOREPLIFETIME(UAbilitySystemComponent, OwnerActor);
	DOREPLIFETIME(UAbilitySystemComponent, AvatarActor);

	DOREPLIFETIME(UAbilitySystemComponent, ReplicatedPredictionKey);
	DOREPLIFETIME(UAbilitySystemComponent, RepAnimMontageInfo);
	
	DOREPLIFETIME_CONDITION(UAbilitySystemComponent, SimulatedTasks, COND_SkipOwner);
}

bool UAbilitySystemComponent::ReplicateSubobjects(class UActorChannel *Channel, class FOutBunch *Bunch, FReplicationFlags *RepFlags)
{
	bool WroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

	for (const UAttributeSet* Set : SpawnedAttributes)
	{
		if (Set)
		{
			WroteSomething |= Channel->ReplicateSubobject(const_cast<UAttributeSet*>(Set), *Bunch, *RepFlags);
		}
	}

	for (UGameplayAbility* Ability : AllReplicatedInstancedAbilities)
	{
		if (Ability && !Ability->HasAnyFlags(RF_PendingKill))
		{
			WroteSomething |= Channel->ReplicateSubobject(Ability, *Bunch, *RepFlags);
		}
	}

	if (!RepFlags->bNetOwner)
	{
		for (UAbilityTask* SimulatedTask : SimulatedTasks)
		{
			if (SimulatedTask && !SimulatedTask->HasAnyFlags(RF_PendingKill))
			{
				WroteSomething |= Channel->ReplicateSubobject(SimulatedTask, *Bunch, *RepFlags);
			}
		}
	}

	return WroteSomething;
}

void UAbilitySystemComponent::GetSubobjectsWithStableNamesForNetworking(TArray<UObject*>& Objs)
{
	for (const UAttributeSet* Set : SpawnedAttributes)
	{
		if (Set && Set->IsNameStableForNetworking())
		{
			Objs.Add(const_cast<UAttributeSet*>(Set));
		}
	}
}

void UAbilitySystemComponent::OnRep_GameplayEffects()
{

}

void UAbilitySystemComponent::OnRep_PredictionKey()
{
	// Every predictive action we've done up to and including the current value of ReplicatedPredictionKey needs to be wiped
	FPredictionKeyDelegates::CatchUpTo(ReplicatedPredictionKey.Current);
}

// ---------------------------------------------------------------------------------------

void UAbilitySystemComponent::PrintAllGameplayEffects() const
{
	ABILITY_LOG_SCOPE(TEXT("PrintAllGameplayEffects %s"), *GetName());
	ABILITY_LOG(Log, TEXT("Owner: %s. Avatar: %s"), *GetOwner()->GetName(), *AbilityActorInfo->AvatarActor->GetName());
	ActiveGameplayEffects.PrintAllGameplayEffects();
}

// ------------------------------------------------------------------------

void UAbilitySystemComponent::OnAttributeAggregatorDirty(FAggregator* Aggregator, FGameplayAttribute Attribute)
{
	ActiveGameplayEffects.OnAttributeAggregatorDirty(Aggregator, Attribute);
}

#undef LOCTEXT_NAMESPACE
