// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Customizes a string class reference to look like a UClass property
 */
class FStringClassReferenceCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance() 
	{
		return MakeShareable(new FStringClassReferenceCustomization);
	}

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

private:
	/** @return The class currently set on this reference */
	const UClass* OnGetClass() const;
	/** Set the class used by this reference */
	void OnSetClass(const UClass* NewClass);

	/** Find or load the class associated with the given string */
	static const UClass* StringToClass(const FString& ClassName);

	/** Handle to the struct property being customized */
	TSharedPtr<IPropertyHandle> StructPropertyHandle;
	/** Handle to the class name property we're editing */
	TSharedPtr<IPropertyHandle> ClassNamePropertyHandle;
	/** A cache of the currently resolved value for the class name */
	mutable TWeakObjectPtr<UClass> CachedClassPtr;
};
