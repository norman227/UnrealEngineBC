// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnvironmentQuery/EnvQueryTypes.h"

class FEnvQueryParamInstanceCustomization : public IPropertyTypeCustomization
{
public:

	static TSharedRef<IPropertyTypeCustomization> MakeInstance( );

public:

	// IPropertyTypeCustomization interface

	virtual void CustomizeHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;
	virtual void CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;

protected:

	TSharedPtr<IPropertyHandle> NameProp;
	TSharedPtr<IPropertyHandle> ValueProp;
	TSharedPtr<IPropertyHandle> TypeProp;

	TOptional<float> GetParamNumValue() const;
	void OnParamNumValueChanged(float FloatValue) const;
	EVisibility GetParamNumValueVisibility() const;

	ESlateCheckBoxState::Type GetParamBoolValue() const;
	void OnParamBoolValueChanged(ESlateCheckBoxState::Type BoolValue) const;
	EVisibility GetParamBoolValueVisibility() const;

	FString GetHeaderDesc() const;
	void OnTypeChanged();
	void InitCachedTypes();

	EEnvQueryParam::Type ParamType;
	mutable uint8 CachedBool : 1;
	mutable float CachedFloat;
	mutable int32 CachedInt;
};
