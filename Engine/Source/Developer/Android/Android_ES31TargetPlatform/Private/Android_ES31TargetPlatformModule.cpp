// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidDXT_TargetPlatformModule.cpp: Implements the FAndroidDXT_TargetPlatformModule class.
=============================================================================*/

#include "Android_ES31TargetPlatformPrivatePCH.h"

#define LOCTEXT_NAMESPACE "FAndroid_ES31TargetPlatformModule" 


/**
 * Android cooking platform which cooks only DXT based textures.
 */
class FAndroid_ES31TargetPlatform
	: public FAndroidTargetPlatform<FAndroid_ES31PlatformProperties>
{
	virtual FString GetAndroidVariantName( ) override
	{
		return TEXT("ES31");
	}

	virtual FText DisplayName( ) const override
	{
		return LOCTEXT("Android_ES31", "Android (ES 3.1)");
	}

	virtual FString PlatformName() const override
	{
		return FString(FAndroid_ES31PlatformProperties::PlatformName());
	}

	virtual bool SupportsTextureFormat( FName Format ) const override
	{
		if( Format == AndroidTexFormat::NameDXT1 ||
			Format == AndroidTexFormat::NameDXT5 ||
			Format == AndroidTexFormat::NameAutoDXT )
		{
			return true;
		}
		return false;
	}

	virtual bool SupportedByExtensionsString( const FString& ExtensionsString, const int GLESVersion ) const override
	{
		// GL_EXT_tessellation_shader implies ES3.1
		return (ExtensionsString.Contains(TEXT("GL_EXT_tessellation_shader")));
	}

	virtual bool SupportsFeature( ETargetPlatformFeatures Feature ) const override
	{
		switch (Feature)
		{
		case ETargetPlatformFeatures::DistanceFieldShadows:
			return FAndroidPlatformProperties::SupportsDistanceFieldShadows();

		case ETargetPlatformFeatures::GrayscaleSRGB:
			return FAndroidPlatformProperties::SupportsGrayscaleSRGB();

		case ETargetPlatformFeatures::HighQualityLightmaps:
			return true;

		case ETargetPlatformFeatures::LowQualityLightmaps:
			return true;

		case ETargetPlatformFeatures::MultipleGameInstances:
			return FAndroidPlatformProperties::SupportsMultipleGameInstances();

		case ETargetPlatformFeatures::Packaging:
			return true;

		case ETargetPlatformFeatures::Tessellation:
			return true;

		case ETargetPlatformFeatures::TextureStreaming:
			return FAndroidPlatformProperties::SupportsTextureStreaming();

		case ETargetPlatformFeatures::VertexShaderTextureSampling:
			return true;
		}

		return false;
	}
	
	virtual uint32 MaxGpuSkinBones( ) const override
	{
		return 256;
	}

#if WITH_ENGINE
	virtual void GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const override
	{
		
		static FName NAME_GLSL_310_ES_EXT(TEXT("GLSL_310_ES_EXT"));
		static FName NAME_OPENGL_ES2(TEXT("GLSL_ES2"));

		OutFormats.AddUnique(NAME_GLSL_310_ES_EXT);
		OutFormats.AddUnique(NAME_OPENGL_ES2);
	}

	virtual void GetReflectionCaptureFormats( TArray<FName>& OutFormats ) const override
	{
		OutFormats.Add(FName(TEXT("FullHDR")));
		OutFormats.Add(FName(TEXT("EncodedHDR")));
	}
#endif
};

/**
 * Holds the target platform singleton.
 */
static ITargetPlatform* AndroidTargetSingleton = NULL;


/**
 * Module for the Android target platform.
 */
class FAndroid_ES31TargetPlatformModule
	: public ITargetPlatformModule
{
public:

	/**
	 * Destructor.
	 */
	~FAndroid_ES31TargetPlatformModule( )
	{
		AndroidTargetSingleton = NULL;
	}


public:
	
	// Begin ITargetPlatformModule interface

	virtual ITargetPlatform* GetTargetPlatform() override
	{
		if (AndroidTargetSingleton == NULL)
		{
			AndroidTargetSingleton = new FAndroid_ES31TargetPlatform();
		}
		
		return AndroidTargetSingleton;
	}

	// End ITargetPlatformModule interface
};


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE( FAndroid_ES31TargetPlatformModule, Android_ES31TargetPlatform);
