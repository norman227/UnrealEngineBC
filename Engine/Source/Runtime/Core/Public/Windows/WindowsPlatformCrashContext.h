// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "GenericPlatform/GenericPlatformContext.h"

#include "AllowWindowsPlatformTypes.h"
#include "DbgHelp.h"
#include "HideWindowsPlatformTypes.h"

struct CORE_API FWindowsPlatformCrashContext : public FGenericCrashContext
{
	/** Platform specific constants. */
	enum EConstants
	{
		UE4_MINIDUMP_CRASHCONTEXT = LastReservedStream + 1,
	};

	virtual void AddPlatformSpecificProperties() override
	{
		AddCrashProperty( TEXT( "Platform.IsRunningWindows" ), 1 );
	}
};

typedef FWindowsPlatformCrashContext FPlatformCrashContext;