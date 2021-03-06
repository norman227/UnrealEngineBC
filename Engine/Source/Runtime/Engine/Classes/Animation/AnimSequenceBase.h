// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Abstract base class of animation sequence that can be played and evaluated to produce a pose.
 *
 */

#include "AnimationAsset.h"
#include "SmartName.h"
#include "Skeleton.h"
#include "Curves/CurveBase.h"
#include "AnimLinkableElement.h"
#include "AnimSequenceBase.generated.h"

#define DEFAULT_SAMPLERATE			30.f
#define MINIMUM_ANIMATION_LENGTH	(1/DEFAULT_SAMPLERATE)

namespace EAnimEventTriggerOffsets
{
	enum Type
	{
		OffsetBefore,
		OffsetAfter,
		NoOffset
	};
}

ENGINE_API float GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::Type OffsetType);

/**
 * Triggers an animation notify.  Each AnimNotifyEvent contains an AnimNotify object
 * which has its Notify method called and passed to the animation.
 */
USTRUCT()
struct FAnimNotifyEvent : public FAnimLinkableElement
{
	GENERATED_USTRUCT_BODY()

	/** The user requested time for this notify */
	UPROPERTY()
	float DisplayTime_DEPRECATED;

	/** An offset from the DisplayTime to the actual time we will trigger the notify, as we cannot always trigger it exactly at the time the user wants */
	UPROPERTY()
	float TriggerTimeOffset;

	/** An offset similar to TriggerTimeOffset but used for the end scrub handle of a notify state's duration */
	UPROPERTY()
	float EndTriggerTimeOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AnimNotifyEvent)
	float TriggerWeightThreshold;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=AnimNotifyEvent)
	FName NotifyName;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category=AnimNotifyEvent)
	class UAnimNotify * Notify;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category=AnimNotifyEvent)
	class UAnimNotifyState * NotifyStateClass;

	UPROPERTY()
	float Duration;

	/** Linkable element to use for the end handle representing a notify state duration */
	UPROPERTY()
	FAnimLinkableElement EndLink;

#if WITH_EDITORONLY_DATA
	/** Color of Notify in editor */
	UPROPERTY()
	FColor NotifyColor;

	/** Visual position of notify in the vertical 'tracks' of the notify editor panel */
	UPROPERTY()
	int32 TrackIndex;

#endif // WITH_EDITORONLY_DATA

	FAnimNotifyEvent()
		: DisplayTime_DEPRECATED(0)
		, TriggerTimeOffset(0)
		, EndTriggerTimeOffset(0)
		, TriggerWeightThreshold(ZERO_ANIMWEIGHT_THRESH)
#if WITH_EDITORONLY_DATA
		, Notify(NULL)
#endif // WITH_EDITORONLY_DATA
		, NotifyStateClass(NULL)
		, Duration(0)
#if WITH_EDITORONLY_DATA
		, TrackIndex(0)
#endif // WITH_EDITORONLY_DATA
	{
	}

	/** Updates trigger offset based on a combination of predicted offset and current offset */
	ENGINE_API void RefreshTriggerOffset(EAnimEventTriggerOffsets::Type PredictedOffsetType);

	/** Updates end trigger offset based on a combination of predicted offset and current offset */
	ENGINE_API void RefreshEndTriggerOffset(EAnimEventTriggerOffsets::Type PredictedOffsetType);

	/** Returns the actual trigger time for this notify. In some cases this may be different to the DisplayTime that has been set */
	ENGINE_API float GetTriggerTime() const;

	/** Returns the actual end time for a state notify. In some cases this may be different from DisplayTime + Duration */
	ENGINE_API float GetEndTriggerTime() const;

	ENGINE_API float GetDuration() const;

	ENGINE_API void SetDuration(float NewDuration);

	/** Returns true if this is blueprint derived notifies **/
	bool IsBlueprintNotify() const
	{
		return Notify != NULL || NotifyStateClass != NULL;
	}

	bool operator ==(const FAnimNotifyEvent& Other) const
	{
		return(
			(Notify && Notify == Other.Notify) || 
			(NotifyStateClass && NotifyStateClass == Other.NotifyStateClass) ||
			(!IsBlueprintNotify() && NotifyName == Other.NotifyName)
			);
	}

	ENGINE_API virtual void SetTime(float NewTime, EAnimLinkMethod::Type ReferenceFrame = EAnimLinkMethod::Absolute) override;
};

/**
 * Keyframe position data for one track.  Pos(i) occurs at Time(i).  Pos.Num() always equals Time.Num().
 */
USTRUCT()
struct FAnimNotifyTrack
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName TrackName;

	UPROPERTY()
	FLinearColor TrackColor;

	TArray<FAnimNotifyEvent*> Notifies;

	FAnimNotifyTrack()
		: TrackName(TEXT(""))
		, TrackColor(FLinearColor::White)
	{
	}

	FAnimNotifyTrack(FName InTrackName, FLinearColor InTrackColor)
		: TrackName(InTrackName)
		, TrackColor(InTrackColor)
	{
	}
};

/**
 * Float curve data for one track
 */

USTRUCT()
struct FAnimCurveBase
{
	GENERATED_USTRUCT_BODY()

	// Last observed name of the curve. We store this so we can recover from situations that
	// mean the skeleton doesn't have a mapped name for our UID (such as a user saving the an
	// animation but not the skeleton).
	UPROPERTY()
	FName		LastObservedName;

	FSmartNameMapping::UID CurveUid;

	FAnimCurveBase(){}

	FAnimCurveBase(FName InName)
		: LastObservedName(InName)
	{	
	}

	FAnimCurveBase(USkeleton::AnimCurveUID Uid)
		: CurveUid(Uid)
	{}

	// To be able to use typedef'd types we need to serialize manually
	virtual void Serialize(FArchive& Ar)
	{
		if(Ar.UE4Ver() >= VER_UE4_SKELETON_ADD_SMARTNAMES)
		{
			Ar << CurveUid;
		}
	}
};

/** native enum for curve types **/
enum EAnimCurveFlags
{
	// Used as morph target curve
	ACF_DrivesMorphTarget	= 0x00000001,
	// Used as triggering event
	ACF_TriggerEvent		= 0x00000002,
	// Is editable in Sequence Editor
	ACF_Editable			= 0x00000004,
	// Used as a material curve
	ACF_DrivesMaterial		= 0x00000008,
	// Is a metadata 'curve'
	ACF_Metadata		= 0x00000010,

	// default flag when created
	ACF_DefaultCurve		= ACF_TriggerEvent | ACF_Editable,
	// curves created from Morph Target
	ACF_MorphTargetCurve	= ACF_DrivesMorphTarget
};

USTRUCT()
struct FFloatCurve : public FAnimCurveBase
{
	GENERATED_USTRUCT_BODY()

	/** Curve data for float. */
	UPROPERTY()
	FRichCurve	FloatCurve;

	FFloatCurve(){}

	FFloatCurve(FName InName, int32 InCurveTypeFlags)
		: FAnimCurveBase(InName)
		, CurveTypeFlags(InCurveTypeFlags)
	{
	}

	FFloatCurve(USkeleton::AnimCurveUID Uid, int32 InCurveTypeFlags)
		: FAnimCurveBase(Uid)
		, CurveTypeFlags(InCurveTypeFlags)
	{}

	/**
	 * Set InFlag to bValue
	 */
	ENGINE_API void SetCurveTypeFlag(EAnimCurveFlags InFlag, bool bValue);

	/**
	 * Toggle the value of the specified flag
	 */
	ENGINE_API void ToggleCurveTypeFlag(EAnimCurveFlags InFlag);

	/**
	 * Return true if InFlag is set, false otherwise 
	 */
	ENGINE_API bool GetCurveTypeFlag(EAnimCurveFlags InFlag) const;

	/**
	 * Set CurveTypeFlags to NewCurveTypeFlags
	 * This just overwrites CurveTypeFlags
	 */
	void SetCurveTypeFlags(int32 NewCurveTypeFlags);
	/** 
	 * returns CurveTypeFlags
	 */
	int32 GetCurveTypeFlags() const;

	virtual void Serialize(FArchive& Ar) override
	{
		FAnimCurveBase::Serialize(Ar);
	}

private:

	/** Curve Type Flags */
	UPROPERTY()
	int32		CurveTypeFlags;
};

/**
 * Raw Curve data for serialization
 */
USTRUCT()
struct FRawCurveTracks
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FFloatCurve>		FloatCurves;

	/**
	 * Evaluate curve data at the time CurrentTime, and add to Instance
	 */
	void EvaluateCurveData(class UAnimInstance* Instance, float CurrentTime, float BlendWeight ) const;
	/**
	 * Find curve data based on the curve UID
	 */
	ENGINE_API FFloatCurve * GetCurveData(USkeleton::AnimCurveUID Uid);
	/**
	 * Add new curve from the provided UID and return true if success
	 * bVectorInterpCurve == true, then it will create FVectorCuve, otherwise, FFloatCurve
	 */
	ENGINE_API bool AddCurveData(USkeleton::AnimCurveUID Uid, int32 CurveFlags = ACF_DefaultCurve);
	/**
	 * Delete curve data 
	 */
	ENGINE_API bool DeleteCurveData(USkeleton::AnimCurveUID Uid);
	/**
	 * Duplicate curve data
	 * 
	 */
	ENGINE_API bool DuplicateCurveData(USkeleton::AnimCurveUID ToCopyUid, USkeleton::AnimCurveUID NewUid);

	/**
	 * Updates the LastObservedName field of the curves from the provided name container
	 */
	ENGINE_API void UpdateLastObservedNames(FSmartNameMapping* NameMapping);

	void Serialize(FArchive& Ar);
};

UENUM()
enum ETypeAdvanceAnim
{
	ETAA_Default,
	ETAA_Finished,
	ETAA_Looped
};

UCLASS(abstract, MinimalAPI, BlueprintType)
class UAnimSequenceBase : public UAnimationAsset
{
	GENERATED_UCLASS_BODY()

	/** Animation notifies, sorted by time (earliest notification first). */
	UPROPERTY()
	TArray<struct FAnimNotifyEvent> Notifies;

	/** Length (in seconds) of this AnimSequence if played back with a speed of 1.0. */
	UPROPERTY(Category=Length, AssetRegistrySearchable, VisibleAnywhere)
	float SequenceLength;

	/** Number for tweaking playback rate of this animation globally. */
	UPROPERTY(EditAnywhere, Category=Animation)
	float RateScale;
	
	/**
	 * Raw uncompressed float curve data 
	 */
	UPROPERTY()
	FRawCurveTracks RawCurveData;

#if WITH_EDITORONLY_DATA
	// if you change Notifies array, this will need to be rebuilt
	UPROPERTY()
	TArray<FAnimNotifyTrack> AnimNotifyTracks;
#endif // WITH_EDITORONLY_DATA

	// Begin UObject interface
	virtual void PostLoad() override;
	// End of UObject interface

	/** Sort the Notifies array by time, earliest first. */
	ENGINE_API void SortNotifies();	

	/** 
	 * Retrieves AnimNotifies given a StartTime and a DeltaTime.
	 * Time will be advanced and support looping if bAllowLooping is true.
	 * Supports playing backwards (DeltaTime<0).
	 * Returns notifies between StartTime (exclusive) and StartTime+DeltaTime (inclusive)
	 */
	void GetAnimNotifies(const float& StartTime, const float& DeltaTime, const bool bAllowLooping, TArray<const FAnimNotifyEvent *> & OutActiveNotifies) const;

	/** 
	 * Retrieves AnimNotifies between two time positions. ]PreviousPosition, CurrentPosition]
	 * Between PreviousPosition (exclusive) and CurrentPosition (inclusive).
	 * Supports playing backwards (CurrentPosition<PreviousPosition).
	 * Only supports contiguous range, does NOT support looping and wrapping over.
	 */
	void GetAnimNotifiesFromDeltaPositions(const float& PreviousPosition, const float& CurrentPosition, TArray<const FAnimNotifyEvent *> & OutActiveNotifies) const;

	/** Evaluate curve data to Instance at the time of CurrentTime **/
	ENGINE_API virtual void EvaluateCurveData(class UAnimInstance* Instance, float CurrentTime, float BlendWeight ) const;

	/**
	 * return true if this is valid additive animation
	 * false otherwise
	 */
	virtual bool IsValidAdditive() const { return false; }

#if WITH_EDITOR
	/** Return Number of Frames **/
	virtual int32 GetNumberOfFrames();

	// update anim notify track cache
	ENGINE_API void UpdateAnimNotifyTrackCache();
	
	// @todo document
	ENGINE_API void InitializeNotifyTrack();

	/** Fix up any notifies that are positioned beyond the end of the sequence */
	ENGINE_API void ClampNotifiesAtEndOfSequence();

	/** Calculates what (if any) offset should be applied to the trigger time of a notify given its display time */ 
	virtual EAnimEventTriggerOffsets::Type CalculateOffsetForNotify(float NotifyDisplayTime) const;

	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	
	// Get a pointer to the data for a given Anim Notify
	ENGINE_API uint8* FindNotifyPropertyData(int32 NotifyIndex, UArrayProperty*& ArrayProperty);

#endif	//WITH_EDITORONLY_DATA

	/**
	 * Update Morph Target Data type to new Curve Type
	 */
	ENGINE_API virtual void UpgradeMorphTargetCurves();

	// Begin UAnimationAsset interface
	virtual void TickAssetPlayerInstance(const FAnimTickRecord& Instance, class UAnimInstance* InstanceOwner, FAnimAssetTickContext& Context) const override;
	// this is used in editor only when used for transition getter
	// this doesn't mean max time. In Sequence, this is SequenceLength,
	// but for BlendSpace CurrentTime is normalized [0,1], so this is 1
	virtual float GetMaxCurrentTime() override { return SequenceLength; }
	// End of UAnimationAsset interface

	virtual void OnAssetPlayerTickedInternal(FAnimAssetTickContext &Context, const float PreviousTime, const float MoveDelta, const FAnimTickRecord &Instance, class UAnimInstance* InstanceOwner) const;

	virtual bool HasRootMotion() const { return false; }

	virtual void Serialize(FArchive& Ar) override;

#if WITH_EDITOR
private:
	DECLARE_MULTICAST_DELEGATE( FOnNotifyChangedMulticaster );
	FOnNotifyChangedMulticaster OnNotifyChanged;

public:
	typedef FOnNotifyChangedMulticaster::FDelegate FOnNotifyChanged;

	/** Registers a delegate to be called after notification has changed*/
	ENGINE_API void RegisterOnNotifyChanged(const FOnNotifyChanged& Delegate);
	ENGINE_API void UnregisterOnNotifyChanged(void* Unregister);
	ENGINE_API virtual bool IsValidToPlay() const { return true; }
#endif
};
