// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"
#include "AssetData.h"

//////////////////////////////////////////////////////////////////////////
// UTileMapActorFactory

UTileMapActorFactory::UTileMapActorFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = NSLOCTEXT("Paper2D", "TileMapFactoryDisplayName", "Paper2D Tile Map");
	NewActorClass = APaperTileMapActor::StaticClass();
}

void UTileMapActorFactory::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	APaperTileMapActor* TypedActor = CastChecked<APaperTileMapActor>(NewActor);
	UPaperTileMapRenderComponent* RenderComponent = TypedActor->GetRenderComponent();
	check(RenderComponent);

	if (UPaperTileMap* TileMap = Cast<UPaperTileMap>(Asset))
	{
		GEditor->SetActorLabelUnique(NewActor, TileMap->GetName());

		RenderComponent->UnregisterComponent();
		RenderComponent->TileMap = TileMap;
		RenderComponent->RegisterComponent();
	}
	else if (UPaperTileSet* TileSet = Cast<UPaperTileSet>(Asset))
	{
		GEditor->SetActorLabelUnique(NewActor, TileSet->GetName());

		if (RenderComponent->TileMap != nullptr)
		{
			RenderComponent->UnregisterComponent();
			RenderComponent->TileMap->TileWidth = TileSet->TileWidth;
			RenderComponent->TileMap->TileHeight = TileSet->TileHeight;
			RenderComponent->RegisterComponent();
		}
	}
}

void UTileMapActorFactory::PostCreateBlueprint(UObject* Asset, AActor* CDO)
{
	if (APaperTileMapActor* TypedActor = Cast<APaperTileMapActor>(CDO))
	{
		UPaperTileMapRenderComponent* RenderComponent = TypedActor->GetRenderComponent();
		check(RenderComponent);

		if (UPaperTileMap* TileMap = Cast<UPaperTileMap>(Asset))
		{
			RenderComponent->TileMap = TileMap;
		}
		else if (UPaperTileSet* TileSet = Cast<UPaperTileSet>(Asset))
		{
			if (RenderComponent->TileMap != nullptr)
			{
				RenderComponent->TileMap->TileWidth = TileSet->TileWidth;
				RenderComponent->TileMap->TileHeight = TileSet->TileHeight;
			}
		}
	}
}

bool UTileMapActorFactory::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	if (GetDefault<UPaperRuntimeSettings>()->bEnableTileMapEditing)
	{
		if (AssetData.IsValid())
		{
			UClass* AssetClass = AssetData.GetClass();
			if ((AssetClass != nullptr) && (AssetClass->IsChildOf(UPaperTileMap::StaticClass()) || AssetClass->IsChildOf(UPaperTileSet::StaticClass())))
			{
				return true;
			}
			else
			{
				OutErrorMsg = NSLOCTEXT("Paper2D", "CanCreateActorFrom_NoTileMap", "No tile map was specified.");
				return false;
			}
		}
		else
		{
			return true;
		}
	}
	else
	{
		OutErrorMsg = NSLOCTEXT("Paper2D", "CanCreateActorFrom_Disabled", "Tile map support is disabled in the Paper2D settings.");
		return false;
	}
}
