// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineTypes.h"

#include "PaperTileSet.generated.h"

UCLASS()
class PAPER2D_API UPaperTileSet : public UDataAsset //@TODO: Just to make it easy to spawn for now
{
	GENERATED_UCLASS_BODY()

	// The width of a single tile (in pixels)
	UPROPERTY(Category=TileSet, EditAnywhere, meta=(UIMin=1, ClampMin=1))
	int32 TileWidth;

	// The height of a single tile (in pixels)
	UPROPERTY(Category=TileSet, EditAnywhere, meta=(UIMin=1, ClampMin=1))
	int32 TileHeight;

	// The tile sheet texture associated with this tile set
	UPROPERTY(Category=TileSet, EditAnywhere)
	UTexture2D* TileSheet;

	// The amount of padding around the perimeter of the tile sheet (in pixels)
	UPROPERTY(Category=TileSet, EditAnywhere, meta=(UIMin=0, ClampMin=0))
	int32 Margin;

	// The amount of padding between tiles in the tile sheet (in pixels)
	UPROPERTY(Category = TileSet, EditAnywhere, meta=(UIMin=0, ClampMin=0))
	int32 Spacing;

	// The drawing offset for tiles from this set (in pixels)
	UPROPERTY(Category=TileSet, EditAnywhere, meta=(UIMin=0, ClampMin=0))
	FIntPoint DrawingOffset;

	// UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	// End of UObject interface

public:
	int32 GetTileCount() const;
	int32 GetTileCountX() const;
	int32 GetTileCountY() const;

	bool GetTileUV(int32 TileIndex, /*out*/ FVector2D& Out_TileUV) const;
};
