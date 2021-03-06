// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "ScriptEditorPluginPrivatePCH.h"
#include "ScriptBlueprintCompiler.h"
#include "KismetCompilerModule.h"
#include "ScriptBlueprintEditor.h"

DEFINE_LOG_CATEGORY(LogScriptEditorPlugin);

/**
* Script blueprint editor actions
*/
class FAssetTypeActions_ScriptClass : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_ScriptClass", "Script Class"); }
	virtual FColor GetTypeColor() const override { return FColor(201, 29, 85); }
	virtual UClass* GetSupportedClass() const override { return UScriptBlueprint::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override
	{
		EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

		for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			auto Blueprint = Cast<UBlueprint>(*ObjIt);
			if (Blueprint && Blueprint->SkeletonGeneratedClass && Blueprint->GeneratedClass)
			{
				TSharedRef<FScriptBlueprintEditor> NewBlueprintEditor(new FScriptBlueprintEditor());

				TArray<UBlueprint*> Blueprints;
				Blueprints.Add(Blueprint);
				NewBlueprintEditor->InitScriptBlueprintEditor(Mode, EditWithinLevelEditor, Blueprints, true);
			}
			else
			{
				FMessageDialog::Open(EAppMsgType::Ok, 
					FText::FromString(TEXT("Script Blueprint could not be loaded because it derives from an invalid class. Check to make sure the parent class for this blueprint hasn't been removed!")));
			}
		}
	}
};

/**
* Script blueprint module
*/
class FScriptEditorPlugin : public IScriptEditorPlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:

	/** Callback for the script compiler */
	FReply CompileScript(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions, FCompilerResultsLog& Results, TArray<UObject*>* ObjLoaded);
};

IMPLEMENT_MODULE(FScriptEditorPlugin, ScriptEditorPlugin)

void FScriptEditorPlugin::StartupModule()
{
	// Register asset types
	FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_ScriptClass));

	// Register blueprint compiler
	IKismetCompilerInterface& KismetCompilerModule = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>("KismetCompiler");
	FBlueprintCompileDelegate CompileDelegate;
	CompileDelegate.BindRaw(this, &FScriptEditorPlugin::CompileScript);
	KismetCompilerModule.GetCompilers().Add(CompileDelegate);
}

void FScriptEditorPlugin::ShutdownModule()
{
}

FReply FScriptEditorPlugin::CompileScript(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions, FCompilerResultsLog& Results, TArray<UObject*>* ObjLoaded)
{
	if (UScriptBlueprint* ScriptBlueprint = Cast<UScriptBlueprint>(Blueprint))
	{
		FScriptBlueprintCompiler Compiler(ScriptBlueprint, Results, CompileOptions, ObjLoaded);
		Compiler.Compile();
		check(Compiler.NewClass);

		return FReply::Handled();
	}

	return FReply::Unhandled();
}