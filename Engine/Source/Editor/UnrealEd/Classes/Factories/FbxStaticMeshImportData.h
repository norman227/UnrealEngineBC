// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/**
 * Import data and options used when importing a static mesh from fbx
 */

#pragma once
#include "FbxStaticMeshImportData.generated.h"

UCLASS(config=EditorUserSettings, AutoExpandCategories=(Options), MinimalAPI)
class UFbxStaticMeshImportData : public UFbxMeshImportData
{
	GENERATED_UCLASS_BODY()

	/** For static meshes, enabling this option will combine all meshes in the FBX into a single monolithic mesh in Unreal */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=ImportSettings, meta=(ImportType="StaticMesh"))
	FName StaticMeshLODGroup;

	/** If true, will replace the vertex colors on an existing mesh with the vertex colors from the FBX file */
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category=ImportSettings, meta=(OBJRestrict="true", ImportType="StaticMesh", ToolTip="If enabled, existing vertex colors will be replaced with ones from the imported file"))
	uint32 bReplaceVertexColors:1;

	/** Disabling this option will keep degenerate triangles found.  In general you should leave this option on. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = ImportSettings, meta = (ImportType = "StaticMesh"))
	uint32 bRemoveDegenerates:1;
	
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category=ImportSettings, meta=(ImportType="StaticMesh"))
	uint32 bGenerateLightmapUVs:1;

	/** If checked, one convex hull per UCX_ prefixed collision mesh will be generated instead of decomposing into multiple hulls */
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category=ImportSettings, meta=(OBJRestrict="true", ImportType="StaticMesh"))
	uint32 bOneConvexHullPerUCX:1;

	/** If checked, collision will automatically be generated (ignored if custom collision is imported or used). */
	UPROPERTY(EditAnywhere, config, Category = ImportSettings, meta=(OBJRestrict="true", ImportType="StaticMesh"))
	uint32 bAutoGenerateCollision : 1;

	/** Gets or creates fbx import data for the specified static mesh */
	static UFbxStaticMeshImportData* GetImportDataForStaticMesh(UStaticMesh* StaticMesh, UFbxStaticMeshImportData* TemplateForCreation);

	bool CanEditChange( const UProperty* InProperty ) const;
};



