// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ScriptBlueprintGeneratedClass.h"
#include "ScriptComponent.generated.h"

/** 
 * Script-extendable component class
 */
UCLASS(Blueprintable, hidecategories = (Object, ActorComponent), editinlinenew, meta = (BlueprintSpawnableComponent), ClassGroup = Script, Abstract, EarlyAccessPreview)
class SCRIPTPLUGIN_API UScriptComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

public:

	/**
	* Calls a script-defined function (no arguments)
	* @param FunctionName Name of the function to call
	*/
	UFUNCTION(BlueprintCallable, Category = "Script|Functions")
	virtual bool CallScriptFunction(FString FunctionName);

	// Begin UActorComponent interface.
	virtual void OnRegister() override;
	virtual void InitializeComponent() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void OnUnregister() override;
	// Begin UActorComponent interface.

protected:

	/** Script context (code) */
	FScriptContextBase* Context;
};

