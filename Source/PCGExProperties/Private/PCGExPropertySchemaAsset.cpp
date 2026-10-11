// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExPropertySchemaAsset.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Misc/DataValidation.h"
#endif

#if WITH_EDITOR
FOnPCGExSchemaAssetChanged UPCGExPropertySchemaAsset::OnAnySchemaAssetChanged;

const FPCGExPropertySchemaCollection* UPCGExPropertySchemaAsset::EDITOR_GetRowSchema() const
{
	return &Collection;
}

void UPCGExPropertySchemaAsset::EDITOR_ForEachPropertyRows(TFunctionRef<void(FPCGExPropertyOverrides& Rows, bool bIsImportLayer)> Visitor)
{
	Visitor(Collection.ImportOverrides, true);
}

void UPCGExPropertySchemaAsset::EDITOR_NotifyRowsChanged()
{
	// An import row is what this asset's importers resolve to: they have to hear about it like any other edit.
	(void)MarkPackageDirty();
	NotifyChanged();
}

void UPCGExPropertySchemaAsset::EDITOR_OnImportedSchemaChanged(UPCGExPropertySchemaAsset* Asset)
{
	// No broadcast of its own: whoever resolves through this asset imports the changed one too, and hears from it.
	if (Collection.SyncImportOverridesInPlace())
	{
		(void)MarkPackageDirty();
	}
}

void UPCGExPropertySchemaAsset::PostLoad()
{
	Super::PostLoad();

	// Self-heal for an asset saved before something it imports drifted. Order matters: the reconcile reads each
	// source schema's outer Name + HeaderId, which SyncAllSchemas canonicalizes here, and which the AtLoad variant
	// gets canonicalized at every depth below by post-loading what this asset imports first.
	Collection.SyncAllSchemas();
	Collection.ReconcileImportOverridesAtLoad();
}

void UPCGExPropertySchemaAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// A drag commits once; every importer re-syncing and rebuilding its panel per tick is wasted and tears down
	// the dragged widget. Super still forwards the Interactive event for live preview.
	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive)
	{
		return;
	}

	// Keep HeaderId / PropertyName in sync for any newly added or edited local schemas.
	// Imported assets sync themselves through their own PostEditChangeProperty.
	Collection.SyncAllSchemas();

	NotifyChanged();
}

void UPCGExPropertySchemaAsset::NotifyChanged()
{
	// Cook guard: WITH_EDITOR is still 1 there, and importers dirty their packages.
	TArray<TWeakObjectPtr<UObject>> Importers;
	if (GEditor && !IsRunningCookCommandlet())
	{
		PCGExPropertyRowHost::GetLoadedImporters(this, Importers, /*bIncludeTemplates=*/true);
	}

	auto Relay = [this, &Importers](const bool bSchemaAssets)
	{
		for (const TWeakObjectPtr<UObject>& Importer : Importers)
		{
			UObject* Live = Importer.Get();
			if (Live && Live != this && Live->IsA<UPCGExPropertySchemaAsset>() == bSchemaAssets)
			{
				CastChecked<IPCGExPropertyRowHost>(Live)->EDITOR_OnImportedSchemaChanged(this);
			}
		}
	};

	Relay(true);
	OnSchemaAssetChanged.Broadcast(this);
	Relay(false);
	OnAnySchemaAssetChanged.Broadcast(this);
}

EDataValidationResult UPCGExPropertySchemaAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// Only surface cycles that reach back to this asset -- other cycles in the graph are
	// reported by their own participants' IsDataValid calls.
	TSet<const UPCGExPropertySchemaAsset*> Visited;
	Visited.Add(this);

	TArray<const UPCGExPropertySchemaAsset*> Stack;

	auto TryEnqueue = [&](UPCGExPropertySchemaAsset* Asset)
	{
		if (!Asset)
		{
			return;
		}
		if (Asset == this)
		{
			Context.AddWarning(FText::FromString(FString::Printf(
				TEXT("Schema asset '%s' is part of a cyclic import chain and will be skipped during resolution."),
				*GetPathName())));
			Result = CombineDataValidationResults(Result, EDataValidationResult::Invalid);
			return;
		}
		bool bAlreadyVisited = false;
		Visited.Add(Asset, &bAlreadyVisited);
		if (!bAlreadyVisited)
		{
			Stack.Add(Asset);
		}
	};

	for (const TObjectPtr<UPCGExPropertySchemaAsset>& AssetPtr : Collection.ImportedSchemas)
	{
		TryEnqueue(AssetPtr.Get());
	}

	while (!Stack.IsEmpty())
	{
		const UPCGExPropertySchemaAsset* Current = Stack.Pop(EAllowShrinking::No);
		for (const TObjectPtr<UPCGExPropertySchemaAsset>& AssetPtr : Current->Collection.ImportedSchemas)
		{
			TryEnqueue(AssetPtr.Get());
		}
	}

	return Result;
}
#endif
