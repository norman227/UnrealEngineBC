// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Engine/BlueprintGeneratedClass.h"
#include "WidgetBlueprintGeneratedClass.generated.h"

class UMovieScene;
class UStructProperty;
class UUserWidget;
class UWidgetAnimation;

UENUM()
namespace EBindingKind
{
	enum Type
	{
		Function,
		Property,
	};
}

USTRUCT()
struct FDelegateRuntimeBinding
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString ObjectName;

	UPROPERTY()
	FName PropertyName;

	UPROPERTY()
	FName FunctionName;

	UPROPERTY()
	TEnumAsByte<EBindingKind::Type> Kind;
	
	/*UFunction* UK2Node_CallFunction::GetTargetFunction() const
	{
	UFunction* Function = FunctionReference.ResolveMember<UFunction>(this);
	return Function;
	}*/
};


/**
 * The widget blueprint generated class allows us to create blueprintable widgets for UMG at runtime.
 * All WBPGC's are of UUserWidget classes, and they perform special post initialization using this class
 * to give themselves many of the same capabiltities as AActor blueprints, like dynamic delegate binding for
 * widgets.
 */
UCLASS()
class UMG_API UWidgetBlueprintGeneratedClass : public UBlueprintGeneratedClass
{
	GENERATED_UCLASS_BODY()

	/** A tree of the widget templates to be created */
	UPROPERTY()
	class UWidgetTree* WidgetTree;

	UPROPERTY()
	TArray< FDelegateRuntimeBinding > Bindings;

	UPROPERTY()
	TArray<UWidgetAnimation*> Animations;

	UPROPERTY()
	TArray< FName > NamedSlots;

	/** This is transient data calculated at link time. */
	TArray<UStructProperty*> WidgetNodeProperties;

	virtual void PostInitProperties() override;

	virtual void Link(FArchive& Ar, bool bRelinkExistingProperties) override;

	/**
	 * This is the function that makes UMG work.  Once a user widget is constructed, it will post load
	 * call into its generated class and ask to be initialized.  The class will perform all the delegate
	 * binding and wiring nessesary to have the user's widget perform as desired.
	 */
	void InitializeWidget(UUserWidget* UserWidget) const;
};