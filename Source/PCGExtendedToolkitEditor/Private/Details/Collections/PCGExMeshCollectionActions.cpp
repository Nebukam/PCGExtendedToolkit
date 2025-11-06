// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/Collections/PCGExMeshCollectionActions.h"

#include "ToolMenuSection.h"
#include "Collections/PCGExMeshCollection.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Views/SListView.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "FileHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Details/Collections/PCGExAssetCollectionEditor.h"
#include "Details/Collections/PCGExMeshCollectionEditor.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"

namespace PCGExMeshCollectionActions
{
	void CreateCollectionFrom(const TArray<FAssetData>& SelectedAssets)
	{
		if (SelectedAssets.IsEmpty()) { return; }

		//FPCGAssetExporterParameters Parameters = InParameters;

		if (SelectedAssets.Num() > 1)
		{
			//Parameters.bOpenSaveDialog = false;
		}

		FString CollectionAssetName = TEXT("SMC_NewMeshCollection");
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

		UPCGExMeshCollection* TargetCollection = nullptr;
		bool bIsNewCollection = false;

		if (Package)
		{
			UObject* Object = FindObjectFast<UObject>(Package, *CollectionAssetName);
			if (Object && Object->GetClass() != UPCGExMeshCollection::StaticClass())
			{
				Object->SetFlags(RF_Transient);
				Object->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
				bIsNewCollection = true;
			}
			else
			{
				TargetCollection = Cast<UPCGExMeshCollection>(Object);
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
			TargetCollection = NewObject<UPCGExMeshCollection>(Package, UPCGExMeshCollection::StaticClass(), FName(*CollectionAssetName), Flags);
		}

		if (TargetCollection)
		{
			if (bIsNewCollection)
			{
				// Notify the asset registry
				FAssetRegistryModule::AssetCreated(TargetCollection);
			}

			TArray<TObjectPtr<UPCGExMeshCollection>> SelectedCollections;
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
		const TArray<TObjectPtr<UPCGExMeshCollection>>& SelectedCollections,
		const TArray<FAssetData>& SelectedAssets,
		bool bIsNewCollection)
	{
		if (SelectedCollections.IsEmpty() || SelectedAssets.IsEmpty()) { return; }

		for (const TObjectPtr<UPCGExMeshCollection>& Collection : SelectedCollections)
		{
			Collection->EDITOR_AddBrowserSelectionTyped(SelectedAssets);
		}
	}
}

FText FPCGExMeshCollectionActions::GetName() const
{
	return INVTEXT("PCGEx Mesh Collection");
}

FString FPCGExMeshCollectionActions::GetObjectDisplayName(UObject* Object) const
{
	return Object->GetName();
}

UClass* FPCGExMeshCollectionActions::GetSupportedClass() const
{
	return UPCGExMeshCollection::StaticClass();
}

FColor FPCGExMeshCollectionActions::GetTypeColor() const
{
	return FColor(0, 255, 255);
}

uint32 FPCGExMeshCollectionActions::GetCategories()
{
	return EAssetTypeCategories::Misc;
}

bool FPCGExMeshCollectionActions::HasActions(const TArray<UObject*>& InObjects) const
{
	return false;
}

void FPCGExMeshCollectionActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	for (UObject* Obj : InObjects)
	{
		if (UPCGExMeshCollection* Collection = Cast<UPCGExMeshCollection>(Obj))
		{
			TSharedRef<FPCGExMeshCollectionEditor> Editor = MakeShared<FPCGExMeshCollectionEditor>();
			Editor->InitEditor(Collection, EToolkitMode::Standalone, EditWithinLevelEditor);
		}
	}
}
