// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/Collections/PCGExPCGDataAssetCollectionActions.h"

#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "FileHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Collections/PCGExPCGDataAssetCollection.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "ToolMenuSection.h"
#include "Details/Collections/PCGExPCGDataAssetCollectionEditor.h"
#include "Details/Collections/PCGExAssetCollectionEditor.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Views/SListView.h"

namespace PCGExPCGDataAssetCollectionActions
{
	void CreateCollectionFrom(const TArray<FAssetData>& SelectedAssets)
	{
		if (SelectedAssets.IsEmpty()) { return; }

		//FPCGAssetExporterParameters Parameters = InParameters;

		if (SelectedAssets.Num() > 1)
		{
			//Parameters.bOpenSaveDialog = false;
		}

		FString CollectionAssetName = TEXT("SMC_NewPCGDataAssetCollection");
		FString CollectionAssetPath = SelectedAssets[0].PackagePath.ToString();
		FString PackageName = FPaths::Combine(CollectionAssetPath, CollectionAssetName);

		/*
		if (Parameters.bOpenSaveDialog)
		{
			FSaveAssetDialogConfig SaveAssetDialogConfig;
			SaveAssetDialogConfig.DefaultPath = AssetPath;
			SaveAssetDialogConfig.DefaultAssetName = AssetName;
			SaveAssetDialogConfig.AssetClassNames.Add(AssetClass->GetClassPathName());
			SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;
			SaveAssetDialogConfig.DialogTitleOverride = NSLOCTEXT("PCGAssetExporter", "SaveAssetToFileDialogTitle", "Save PCG Asset");
	
			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);
			if (!SaveObjectPath.IsEmpty())
			{
				AssetName = FPackageName::ObjectPathToObjectName(SaveObjectPath);
				AssetPath = FString(); // not going to be reused
				PackageName = FPackageName::ObjectPathToPackageName(SaveObjectPath);
			}
			else
			{
				return nullptr;
			}
		}
		else
		{
			*/
		// Perform some validation on the package name, so we can prevent crashes downstream when trying to create or save the package.
		FText Reason;
		if (!FPackageName::IsValidObjectPath(PackageName, &Reason))
		{
			UE_LOG(LogTemp, Error, TEXT("Invalid package path '%s': %s."), *PackageName, *Reason.ToString());
			return;
		}
		//}

		UPackage* Package = FPackageName::DoesPackageExist(PackageName) ? LoadPackage(nullptr, *PackageName, LOAD_None) : nullptr;

		UPCGExPCGDataAssetCollection* TargetCollection = nullptr;
		bool bIsNewCollection = false;

		if (Package)
		{
			UObject* Object = FindObjectFast<UObject>(Package, *CollectionAssetName);
			if (Object && Object->GetClass() != UPCGExPCGDataAssetCollection::StaticClass())
			{
				Object->SetFlags(RF_Transient);
				Object->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
				bIsNewCollection = true;
			}
			else
			{
				TargetCollection = Cast<UPCGExPCGDataAssetCollection>(Object);
			}
		}
		else
		{
			Package = CreatePackage(*PackageName);

			if (Package)
			{
				bIsNewCollection = true;
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Unable to create package with name '%s'."), *PackageName);
				return;
			}
		}

		if (!TargetCollection)
		{
			constexpr EObjectFlags Flags = RF_Public | RF_Standalone | RF_Transactional;
			TargetCollection = NewObject<UPCGExPCGDataAssetCollection>(Package, UPCGExPCGDataAssetCollection::StaticClass(), FName(*CollectionAssetName), Flags);
		}

		if (TargetCollection)
		{
			if (bIsNewCollection)
			{
				// Notify the asset registry
				FAssetRegistryModule::AssetCreated(TargetCollection);
			}

			TArray<TObjectPtr<UPCGExPCGDataAssetCollection>> SelectedCollections;
			SelectedCollections.Add(TargetCollection);

			UpdateCollectionsFrom(SelectedCollections, SelectedAssets, bIsNewCollection);
		}

		// Save the file
		if (Package) // && Parameters.bSaveOnExportEnded)
		{
			FEditorFileUtils::PromptForCheckoutAndSave({Package}, /*bCheckDirty=*/false, /*bPromptToSave=*/false);
		}
	}

	void UpdateCollectionsFrom(
		const TArray<TObjectPtr<UPCGExPCGDataAssetCollection>>& SelectedCollections,
		const TArray<FAssetData>& SelectedAssets,
		bool bIsNewCollection)
	{
		if (SelectedCollections.IsEmpty() || SelectedAssets.IsEmpty()) { return; }

		for (const TObjectPtr<UPCGExPCGDataAssetCollection>& Collection : SelectedCollections)
		{
			Collection->EDITOR_AddBrowserSelectionTyped(SelectedAssets);
		}
	}
}

FText FPCGExPCGDataAssetCollectionActions::GetName() const
{
	return INVTEXT("PCGEx PCGDataAsset Collection");
}

FString FPCGExPCGDataAssetCollectionActions::GetObjectDisplayName(UObject* Object) const
{
	return Object->GetName();
}

UClass* FPCGExPCGDataAssetCollectionActions::GetSupportedClass() const
{
	return UPCGExPCGDataAssetCollection::StaticClass();
}

FColor FPCGExPCGDataAssetCollectionActions::GetTypeColor() const
{
	return FColor(100, 150, 200);
}

uint32 FPCGExPCGDataAssetCollectionActions::GetCategories()
{
	return EAssetTypeCategories::Misc;
}

bool FPCGExPCGDataAssetCollectionActions::HasActions(const TArray<UObject*>& InObjects) const
{
	return false;
}

void FPCGExPCGDataAssetCollectionActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	for (UObject* Obj : InObjects)
	{
		if (UPCGExPCGDataAssetCollection* Collection = Cast<UPCGExPCGDataAssetCollection>(Obj))
		{
			TSharedRef<FPCGExPCGDataAssetCollectionEditor> Editor = MakeShared<FPCGExPCGDataAssetCollectionEditor>();
			Editor->InitEditor(Collection, EToolkitMode::Standalone, EditWithinLevelEditor);
		}
	}
}
