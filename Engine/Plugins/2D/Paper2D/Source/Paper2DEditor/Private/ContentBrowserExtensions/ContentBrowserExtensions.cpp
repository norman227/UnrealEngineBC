// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"
#include "ContentBrowserModule.h"
#include "ContentBrowserExtensions.h"

#define LOCTEXT_NAMESPACE "Paper2D"

DECLARE_LOG_CATEGORY_EXTERN(LogPaperCBExtensions, Log, All);
DEFINE_LOG_CATEGORY(LogPaperCBExtensions);

//////////////////////////////////////////////////////////////////////////

FContentBrowserMenuExtender_SelectedAssets ContentBrowserExtenderDelegate;

//////////////////////////////////////////////////////////////////////////
// FContentBrowserSelectedAssetExtensionBase

struct FContentBrowserSelectedAssetExtensionBase
{
public:
	TArray<class FAssetData> SelectedAssets;

public:
	virtual void Execute() {}
	virtual ~FContentBrowserSelectedAssetExtensionBase() {}
};

//////////////////////////////////////////////////////////////////////////
// FCreateSpriteFromTextureExtension

#include "AssetToolsModule.h"
#include "AssetRegistryModule.h"


struct FCreateSpriteFromTextureExtension : public FContentBrowserSelectedAssetExtensionBase
{
	bool bExtractSprites;

	FCreateSpriteFromTextureExtension()
		: bExtractSprites(false)
	{
	}

	void CreateSpritesFromTextures(TArray<UTexture2D*>& Textures)
	{
		const FString DefaultSuffix = TEXT("_Sprite");

		FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

		const bool bOneTextureSelected = Textures.Num() == 1;
		TArray<UObject*> ObjectsToSync;

		FString Name;
		FString PackageName;

		for (auto TextureIt = Textures.CreateConstIterator(); TextureIt; ++TextureIt)
		{
			UTexture2D* Texture = *TextureIt;

			// Create the factory used to generate the sprite
			UPaperSpriteFactory* SpriteFactory = ConstructObject<UPaperSpriteFactory>(UPaperSpriteFactory::StaticClass());
			SpriteFactory->InitialTexture = Texture;

			// Create the sprite
			if (!bExtractSprites)
			{
				if (bOneTextureSelected)
				{
					// Get a unique name for the sprite
					AssetToolsModule.Get().CreateUniqueAssetName(Texture->GetOutermost()->GetName(), DefaultSuffix, /*out*/ PackageName, /*out*/ Name);
					const FString PackagePath = FPackageName::GetLongPackagePath(PackageName);
					ContentBrowserModule.Get().CreateNewAsset(Name, PackagePath, UPaperSprite::StaticClass(), SpriteFactory);
				}
				else
				{
					// Get a unique name for the sprite
					AssetToolsModule.Get().CreateUniqueAssetName(Texture->GetOutermost()->GetName(), DefaultSuffix, /*out*/ PackageName, /*out*/ Name);
					const FString PackagePath = FPackageName::GetLongPackagePath(PackageName);

					if (UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, PackagePath, UPaperSprite::StaticClass(), SpriteFactory))
					{
						ObjectsToSync.Add(NewAsset);
					}
				}
			}
			else
			{
				FScopedSlowTask Feedback(1, NSLOCTEXT("Paper2D", "Paper2D_ExtractSpritesFromTexture", "Extracting Sprites From Texture"));
				Feedback.MakeDialog(true);

				// First extract the rects from the texture
				TArray<FIntRect> ExtractedRects;
				UPaperSprite::ExtractRectsFromTexture(Texture, /*out*/ ExtractedRects);

				Feedback.TotalAmountOfWork = ExtractedRects.Num();

				for (int ExtractedRectIndex = 0; ExtractedRectIndex < ExtractedRects.Num(); ++ExtractedRectIndex)
				{
					Feedback.EnterProgressFrame(1, NSLOCTEXT("Paper2D", "Paper2D_ExtractSpritesFromTexture", "Extracting Sprites From Texture"));

					FIntRect& ExtractedRect = ExtractedRects[ExtractedRectIndex];
					SpriteFactory->bUseSourceRegion = true;
					SpriteFactory->InitialSourceUV = FVector2D(ExtractedRect.Min.X, ExtractedRect.Min.Y);
					SpriteFactory->InitialSourceDimension = FVector2D(ExtractedRect.Width(), ExtractedRect.Height());

					// Get a unique name for the sprite
					AssetToolsModule.Get().CreateUniqueAssetName(Texture->GetOutermost()->GetName(), DefaultSuffix, /*out*/ PackageName, /*out*/ Name);
					const FString PackagePath = FPackageName::GetLongPackagePath(PackageName);

					if (UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, PackagePath, UPaperSprite::StaticClass(), SpriteFactory))
					{
						ObjectsToSync.Add(NewAsset);
					}

					if (GWarn->ReceivedUserCancel()) 
					{
						break;
					}
				}
			}
		}

		if (ObjectsToSync.Num() > 0)
		{
			ContentBrowserModule.Get().SyncBrowserToAssets(ObjectsToSync);
		}
	}

	virtual void Execute() override
	{
		// Create sprites for any selected textures
		TArray<UTexture2D*> Textures;
		for (auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt)
		{
			const FAssetData& AssetData = *AssetIt;
			if (UTexture2D* Texture = Cast<UTexture2D>(AssetData.GetAsset()))
			{
				Textures.Add(Texture);
			}
		}

		CreateSpritesFromTextures(Textures);
	}
};

struct FConfigureTexturesForSpriteUsageExtension : public FContentBrowserSelectedAssetExtensionBase
{
	virtual void Execute() override
	{
		// Change the compression settings and trigger a recompress
		for (auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt)
		{
			const FAssetData& AssetData = *AssetIt;
			if (UTexture2D* Texture = Cast<UTexture2D>(AssetData.GetAsset()))
			{
				Texture->Modify();
				Texture->LODGroup = TEXTUREGROUP_UI;
				Texture->CompressionSettings = TC_EditorIcon;
				Texture->Filter = TF_Nearest;
				Texture->PostEditChange();
			}
		}
	}
};

//////////////////////////////////////////////////////////////////////////
// FPaperContentBrowserExtensions_Impl

class FPaperContentBrowserExtensions_Impl
{
public:
	static void ExecuteSelectedContentFunctor(TSharedPtr<FContentBrowserSelectedAssetExtensionBase> SelectedAssetFunctor)
	{
		SelectedAssetFunctor->Execute();
	}

	static void CreateSpriteActionsSubMenu(FMenuBuilder& MenuBuilder, TSharedPtr<FCreateSpriteFromTextureExtension> SpriteCreator, TSharedPtr<FCreateSpriteFromTextureExtension> SpriteExtractor, TSharedPtr<FConfigureTexturesForSpriteUsageExtension> ConfigFunctor)
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("SpriteActionsSubMenuLabel", "Sprite Actions"),
			LOCTEXT("SpriteActionsSubMenuToolTip", "Sprite-related actions for this texture."),
			FNewMenuDelegate::CreateStatic(&FPaperContentBrowserExtensions_Impl::PopulateSpriteActionsMenu, SpriteCreator, SpriteExtractor, ConfigFunctor),
			false,
			FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.PaperSprite")
		);
	}

	static void PopulateSpriteActionsMenu(FMenuBuilder& MenuBuilder, TSharedPtr<FCreateSpriteFromTextureExtension> SpriteCreator, TSharedPtr<FCreateSpriteFromTextureExtension> SpriteExtractor, TSharedPtr<FConfigureTexturesForSpriteUsageExtension> ConfigFunctor)
	{
		// Create sprites
		FUIAction Action_CreateSpritesFromTextures(
			FExecuteAction::CreateStatic(&FPaperContentBrowserExtensions_Impl::ExecuteSelectedContentFunctor, StaticCastSharedPtr<FContentBrowserSelectedAssetExtensionBase>(SpriteCreator)));
		
		MenuBuilder.AddMenuEntry(
			LOCTEXT("CB_Extension_Texture_CreateSprite", "Create Sprite"),
			LOCTEXT("CB_Extension_Texture_CreateSprite_Tooltip", "Create sprites from selected textures"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.PaperSprite"),
			Action_CreateSpritesFromTextures,
			NAME_None,
			EUserInterfaceActionType::Button);

		// Extract Sprites
		FUIAction Action_ExtractSpritesFromTextures(
			FExecuteAction::CreateStatic(&FPaperContentBrowserExtensions_Impl::ExecuteSelectedContentFunctor, StaticCastSharedPtr<FContentBrowserSelectedAssetExtensionBase>(SpriteExtractor)));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("CB_Extension_Texture_ExtractSprites", "Extract Sprites"),
			LOCTEXT("CB_Extension_Texture_ExtractSprite_Tooltip", "Detects and extracts sprites from the selected textures using transparency"),
			FSlateIcon(),
			Action_ExtractSpritesFromTextures,
			NAME_None,
			EUserInterfaceActionType::Button);

		// Configure for retro sprites
		FUIAction Action_ConfigureTexturesForSprites(
			FExecuteAction::CreateStatic(&FPaperContentBrowserExtensions_Impl::ExecuteSelectedContentFunctor, StaticCastSharedPtr<FContentBrowserSelectedAssetExtensionBase>(ConfigFunctor)));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("CB_Extension_Texture_ConfigureTextureForSprites", "Configure For Retro Sprites"),
			LOCTEXT("CB_Extension_Texture_ConfigureTextureForSprites_Tooltip", "Sets compression settings and sampling modes to good defaults for retro sprites (nearest filtering, uncompressed, etc...)"),
			FSlateIcon(),
			Action_ConfigureTexturesForSprites,
			NAME_None,
			EUserInterfaceActionType::Button);
	}

	static TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets)
	{
		TSharedRef<FExtender> Extender(new FExtender());

		// Run thru the assets to determine if any meet our criteria
		bool bAnyTextures = false;
		for (auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt)
		{
			const FAssetData& Asset = *AssetIt;
			bAnyTextures = bAnyTextures || (Asset.AssetClass == UTexture2D::StaticClass()->GetFName());
		}

		if (bAnyTextures)
		{
			// Create Sprite
			TSharedPtr<FCreateSpriteFromTextureExtension> SpriteCreatorFunctor = MakeShareable(new FCreateSpriteFromTextureExtension());
			SpriteCreatorFunctor->SelectedAssets = SelectedAssets;

			////@TODO: Already getting nasty, need to refactor the data to be independent of the functor
			// Extract Sprites
			TSharedPtr<FCreateSpriteFromTextureExtension> SpriteExtractorFunctor = MakeShareable(new FCreateSpriteFromTextureExtension());
			SpriteExtractorFunctor->SelectedAssets = SelectedAssets;
			SpriteExtractorFunctor->bExtractSprites = true;

			// Configure for retro sprites
			TSharedPtr<FConfigureTexturesForSpriteUsageExtension> TextureConfigFunctor = MakeShareable(new FConfigureTexturesForSpriteUsageExtension());
			TextureConfigFunctor->SelectedAssets = SelectedAssets;

			// Add the sprite actions sub-menu extender
			Extender->AddMenuExtension(
				"GetAssetActions",
				EExtensionHook::After,
				NULL,
				FMenuExtensionDelegate::CreateStatic(&FPaperContentBrowserExtensions_Impl::CreateSpriteActionsSubMenu, SpriteCreatorFunctor, SpriteExtractorFunctor, TextureConfigFunctor));
		}

		return Extender;
	}

	static TArray<FContentBrowserMenuExtender_SelectedAssets>& GetExtenderDelegates()
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		return ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
	}
};

//////////////////////////////////////////////////////////////////////////
// FPaperContentBrowserExtensions

void FPaperContentBrowserExtensions::InstallHooks()
{
	ContentBrowserExtenderDelegate = FContentBrowserMenuExtender_SelectedAssets::CreateStatic(&FPaperContentBrowserExtensions_Impl::OnExtendContentBrowserAssetSelectionMenu);

	TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = FPaperContentBrowserExtensions_Impl::GetExtenderDelegates();
	CBMenuExtenderDelegates.Add(ContentBrowserExtenderDelegate);
}

void FPaperContentBrowserExtensions::RemoveHooks()
{
	TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = FPaperContentBrowserExtensions_Impl::GetExtenderDelegates();
	CBMenuExtenderDelegates.Remove(ContentBrowserExtenderDelegate);
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE