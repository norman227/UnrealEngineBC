// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "Paper2DPrivatePCH.h"
#include "PaperTileMapRenderSceneProxy.h"
#include "PhysicsEngine/BodySetup.h"
#include "PaperCustomVersion.h"

#define LOCTEXT_NAMESPACE "Paper2D"

//////////////////////////////////////////////////////////////////////////
// UPaperTileMapRenderComponent

UPaperTileMapRenderComponent::UPaperTileMapRenderComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BodyInstance.SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	MapWidth_DEPRECATED = 4;
	MapHeight_DEPRECATED = 4;
	TileWidth_DEPRECATED = 32;
	TileHeight_DEPRECATED = 32;

	static ConstructorHelpers::FObjectFinder<UMaterial> DefaultMaterial(TEXT("/Paper2D/DefaultSpriteMaterial"));
	Material_DEPRECATED = DefaultMaterial.Object;
}

FPrimitiveSceneProxy* UPaperTileMapRenderComponent::CreateSceneProxy()
{
	FPaperTileMapRenderSceneProxy* Proxy = new FPaperTileMapRenderSceneProxy(this);
	RebuildRenderData(Proxy);

	return Proxy;
}

void UPaperTileMapRenderComponent::PostInitProperties()
{
	TileMap = NewObject<UPaperTileMap>(this);
	TileMap->SetFlags(RF_Transactional);
	Super::PostInitProperties();
}

FBoxSphereBounds UPaperTileMapRenderComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (TileMap != NULL)
	{
		// Graphics bounds.
		FBoxSphereBounds NewBounds = TileMap->GetRenderBounds().TransformBy(LocalToWorld);

		// Add bounds of collision geometry (if present).
		if (UBodySetup* BodySetup = TileMap->BodySetup)
		{
			const FBox AggGeomBox = BodySetup->AggGeom.CalcAABB(LocalToWorld);
			if (AggGeomBox.IsValid)
			{
				NewBounds = Union(NewBounds, FBoxSphereBounds(AggGeomBox));
			}
		}

		// Apply bounds scale
		NewBounds.BoxExtent *= BoundsScale;
		NewBounds.SphereRadius *= BoundsScale;

		return NewBounds;
	}
	else
	{
		return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.f);
	}
}

void UPaperTileMapRenderComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// Indicate we need to send new dynamic data.
	MarkRenderDynamicDataDirty();
}

#if WITH_EDITORONLY_DATA
void UPaperTileMapRenderComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FPaperCustomVersion::GUID);
}

void UPaperTileMapRenderComponent::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	if (GetLinkerCustomVersion(FPaperCustomVersion::GUID) < FPaperCustomVersion::MovedTileMapDataToSeparateClass)
	{
		// Create a tile map object and move our old properties over to it
		TileMap = NewObject<UPaperTileMap>(this);
		TileMap->SetFlags(RF_Transactional);
		TileMap->MapWidth = MapWidth_DEPRECATED;
		TileMap->MapHeight = MapHeight_DEPRECATED;
		TileMap->TileWidth = TileWidth_DEPRECATED;
		TileMap->TileHeight = TileHeight_DEPRECATED;
		TileMap->PixelsPerUnit = 1.0f;
		TileMap->DefaultLayerTileSet = DefaultLayerTileSet_DEPRECATED;
		TileMap->Material = Material_DEPRECATED;
		TileMap->TileLayers = TileLayers_DEPRECATED;

		// Convert the layers
		for (UPaperTileLayer* Layer : TileMap->TileLayers)
		{
			Layer->Rename(nullptr, TileMap, REN_ForceNoResetLoaders | REN_DontCreateRedirectors);
			Layer->ConvertToTileSetPerCell();
		}

		// Remove references in the deprecated variables to prevent issues with deleting referenced assets, etc...
		DefaultLayerTileSet_DEPRECATED = nullptr;
		Material_DEPRECATED = nullptr;
		TileLayers_DEPRECATED.Empty();
	}
#endif
}
#endif

UBodySetup* UPaperTileMapRenderComponent::GetBodySetup()
{
	return (TileMap != nullptr) ? TileMap->BodySetup : nullptr;
}

const UObject* UPaperTileMapRenderComponent::AdditionalStatObject() const
{
	if (TileMap != nullptr)
	{
		if (TileMap->GetOuter() != this)
		{
			return TileMap;
		}
	}

	return nullptr;
}

void UPaperTileMapRenderComponent::RebuildRenderData(FPaperTileMapRenderSceneProxy* Proxy)
{
	TArray<FSpriteDrawCallRecord> BatchedSprites;
	
	if (TileMap == nullptr)
	{
		return;
	}

	for (int32 Z = 0; Z < TileMap->TileLayers.Num(); ++Z)
	{
		UPaperTileLayer* Layer = TileMap->TileLayers[Z];

		if (Layer == nullptr)
		{
			continue;
		}

		FLinearColor DrawColor = FLinearColor::White;
#if WITH_EDITORONLY_DATA
		if (Layer->bHiddenInEditor)
		{
			continue;
		}

		DrawColor.A = Layer->LayerOpacity;
#endif

		for (int32 Y = 0; Y < TileMap->MapHeight; ++Y)
		{
			// In pixels
			FVector WorldOffset = FVector::ZeroVector;
			FVector WorldStepX;
			FVector WorldStepY;
			FVector TileSetOffset = FVector::ZeroVector;
			switch (TileMap->ProjectionMode)
			{
			case ETileMapProjectionMode::Orthogonal:
			default:
				WorldOffset = (TileMap->TileWidth * PaperAxisX * 0.5f) + (TileMap->TileHeight * PaperAxisY * 0.5f);
				WorldStepX = PaperAxisX * TileMap->TileWidth;
				WorldStepY = -PaperAxisY * TileMap->TileHeight;
				break;
			case ETileMapProjectionMode::IsometricDiamond:
				WorldOffset = (TileMap->MapWidth * TileMap->TileWidth * 0.5f * PaperAxisX) - (Y * TileMap->TileHeight * 0.5f * PaperAxisY);
				WorldStepX = (TileMap->TileWidth * PaperAxisX * 0.5f) - (TileMap->TileHeight * PaperAxisY * 0.5f);
				WorldStepY = (TileMap->TileWidth * PaperAxisX * -0.5f) - (TileMap->TileHeight * PaperAxisY * 0.5f);
				break;
			case ETileMapProjectionMode::IsometricStaggered:
				WorldOffset = ((Y & 1) * 0.5f * TileMap->TileWidth * PaperAxisX) + (0.5f * TileMap->TileHeight * PaperAxisY);
				WorldStepX = PaperAxisX * TileMap->TileWidth;
				WorldStepY = -PaperAxisY * TileMap->TileHeight;
				break;
			}

			for (int32 X = 0; X < TileMap->MapWidth; ++X)
			{
				const FPaperTileInfo TileInfo = Layer->GetCell(X, Y);

				// do stuff
				const float TotalSeparation = (TileMap->SeparationPerLayer * Z) + (TileMap->SeparationPerTileX * X) + (TileMap->SeparationPerTileY * Y);
				FVector WorldPos = (WorldStepX * X) + (WorldStepY * Y) + WorldOffset;
				WorldPos += TotalSeparation * PaperAxisZ;

				const int32 TileWidth = TileMap->TileWidth;
				const int32 TileHeight = TileMap->TileHeight;

				UTexture2D* LastSourceTexture = nullptr;
				FVector2D InverseTextureSize(1.0f, 1.0f);
				FVector2D SourceDimensionsUV(1.0f, 1.0f);

				FVector2D TileSeparation(TileMap->TileWidth, TileMap->TileHeight);
				FVector2D TileSizeXY(TileMap->TileWidth, TileMap->TileHeight);

				{
					UTexture2D* SourceTexture = nullptr;

					FVector2D SourceUV = FVector2D::ZeroVector;
					if (Layer->bCollisionLayer)
					{
						if (TileInfo.PackedTileIndex == 0)
						{
							continue;
						}
						SourceTexture = UCanvas::StaticClass()->GetDefaultObject<UCanvas>()->DefaultTexture;
					}
					else
					{
						if (TileInfo.TileSet == nullptr)
						{
							continue;
						}

						if (!TileInfo.TileSet->GetTileUV(TileInfo.PackedTileIndex, /*out*/ SourceUV))
						{
							continue;
						}

						SourceTexture = TileInfo.TileSet->TileSheet;
						if (SourceTexture == nullptr)
						{
							continue;
						}
					}

					if (SourceTexture != LastSourceTexture)
					{
						InverseTextureSize = FVector2D(1.0f / SourceTexture->GetSizeX(), 1.0f / SourceTexture->GetSizeY());

						if (TileInfo.TileSet != nullptr)
						{
							SourceDimensionsUV = FVector2D(TileInfo.TileSet->TileWidth * InverseTextureSize.X, TileInfo.TileSet->TileHeight * InverseTextureSize.Y);
							TileSeparation = FVector2D(TileInfo.TileSet->TileWidth, TileInfo.TileSet->TileHeight);
							TileSetOffset = (TileInfo.TileSet->DrawingOffset.X * PaperAxisX) + (TileInfo.TileSet->DrawingOffset.Y * PaperAxisY);
						}
						else
						{
							SourceDimensionsUV = FVector2D(TileWidth * InverseTextureSize.X, TileHeight * InverseTextureSize.Y);
							TileSeparation = FVector2D(TileWidth, TileHeight);
							TileSetOffset = FVector::ZeroVector;
						}
						LastSourceTexture = SourceTexture;
					}

					SourceUV.X *= InverseTextureSize.X;
					SourceUV.Y *= InverseTextureSize.Y;

					FSpriteDrawCallRecord& NewTile = *(new (BatchedSprites) FSpriteDrawCallRecord());
					NewTile.Destination = WorldPos + TileSetOffset;
					NewTile.Texture = SourceTexture;
					NewTile.Color = DrawColor;

					const FVector4 BottomLeft(0.0f, 0.0f, SourceUV.X, SourceUV.Y + SourceDimensionsUV.Y);
					const FVector4 BottomRight(TileSeparation.X, 0.0f, SourceUV.X + SourceDimensionsUV.X, SourceUV.Y + SourceDimensionsUV.Y);
					const FVector4 TopRight(TileSeparation.X, TileSeparation.Y, SourceUV.X + SourceDimensionsUV.X, SourceUV.Y);
					const FVector4 TopLeft(0.0f, TileSeparation.Y, SourceUV.X, SourceUV.Y);

					new (NewTile.RenderVerts) FVector4(BottomLeft);
					new (NewTile.RenderVerts) FVector4(TopRight);
					new (NewTile.RenderVerts) FVector4(BottomRight);

					new (NewTile.RenderVerts) FVector4(BottomLeft);
					new (NewTile.RenderVerts) FVector4(TopLeft);
					new (NewTile.RenderVerts) FVector4(TopRight);
				}
			}
		}
	}

	Proxy->SetBatchesHack(BatchedSprites);
}

#undef LOCTEXT_NAMESPACE