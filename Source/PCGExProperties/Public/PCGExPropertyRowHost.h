// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/StructView.h"
#include "UObject/Interface.h"

#include "PCGExPropertyRowHost.generated.h"

class UPCGExPropertySchemaAsset;
struct FInstancedStruct;
struct FPCGExPropertyOverrides;
struct FPCGExPropertySchemaCollection;
struct FPropertyChangedEvent;

#if WITH_EDITOR
/**
 * Where a row container sits in its host, as the details tree sees it. Built by the row customization from handles,
 * so it describes a panel copy as well as live storage; a host reads it to pick the layers below those rows.
 */
struct FPCGExPropertyRowSite
{
	/** The struct holding the rows member (an entry, a category row, a schema collection). Invalid for a UObject member or an array element. */
	FConstStructView Outer;

	/** Name of the FPCGExPropertyOverrides member the rows are. */
	FName RowsMember = NAME_None;

	/** True when the rows are the schema's own ImportOverrides. */
	bool bIsImportLayer = false;
};
#endif

UINTERFACE()
class PCGEXPROPERTIES_API UPCGExPropertyRowHost : public UInterface
{
	GENERATED_BODY()
};

/**
 * An object that owns override rows against a property schema: a collection (entries, category rows), a Tuple
 * (its values), a property collection component or a schema asset (their import overrides). Editor tooling that
 * walks rows across hosts (the choices align lever, the imported-schema relay) goes through this; the host keeps
 * its row storage private.
 */
class PCGEXPROPERTIES_API IPCGExPropertyRowHost
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	using FLayerChain = TArray<const FPCGExPropertyOverrides*, TInlineAllocator<4>>;

	/** The schema the rows are parallel to. */
	virtual const FPCGExPropertySchemaCollection* EDITOR_GetRowSchema() const = 0;

	/** Layers the schema resolves through below its own import layer (a Blueprint class chain). None for most hosts. */
	virtual void EDITOR_GetRowSchemaFallbackChain(FLayerChain& OutChain) const
	{
	}

	/**
	 * Visit every row container; bIsImportLayer marks the schema's own ImportOverrides, whose rows are part of what
	 * the schema resolves to. The visitor may write values in place and never reshapes a container; calling Modify()
	 * on the host before its first write is the visitor's job.
	 */
	virtual void EDITOR_ForEachPropertyRows(TFunctionRef<void(FPCGExPropertyOverrides& Rows, bool bIsImportLayer)> Visitor) = 0;

	/** Row values were written in place: dirty, refresh views, re-run whatever the host derives from its rows. */
	virtual void EDITOR_NotifyRowsChanged() = 0;

	/**
	 * The value PropertyName takes for the rows at Site while their own slot is disabled: what the host's consumers
	 * fall through to, choices baked. The default resolves the row schema through the fallback chain, own layer left
	 * out for the import layer. False, Out untouched, when nothing below Site declares the name.
	 */
	virtual bool EDITOR_ResolveInheritedValue(const FPCGExPropertyRowSite& Site, FName PropertyName, FInstancedStruct& Out) const;

	/**
	 * A schema asset this host's schema imports changed. Bring the rows back in line without reshaping the schema's
	 * own import layer, which may be aliased by a live panel: that reshape is the schema customization's.
	 */
	virtual void EDITOR_OnImportedSchemaChanged(UPCGExPropertySchemaAsset* Asset) = 0;
#endif
};

#if WITH_EDITOR
namespace PCGExPropertyRowHost
{
	/**
	 * Every loaded row host whose schema imports Asset, at any depth. Collected up front and handed out weak, so the
	 * caller may run code that creates or destroys objects while it walks the result. Class defaults, transient,
	 * play-in-editor and garbage objects are always left out; Blueprint templates unless bIncludeTemplates.
	 */
	PCGEXPROPERTIES_API void GetLoadedImporters(const UPCGExPropertySchemaAsset* Asset, TArray<TWeakObjectPtr<UObject>>& OutHosts, bool bIncludeTemplates = false);

	/** Tell listeners keyed on Host that its RowsMember array was rebuilt. The details view is not one of them. */
	PCGEXPROPERTIES_API void BroadcastRowsRebuilt(UObject* Host, FName RowsMember);

	/** What an edit asks of a host built as one schema collection member plus one array member of rows. */
	struct FEditKind
	{
		/** Rows have to be brought back in line with the schema. */
		bool bSyncRows = false;
		/** The schema itself changed, not just the shape of the rows array. */
		bool bSchemaChanged = false;
	};

	/** Classify Event for such a host. A drag in progress (Interactive) asks for nothing: it commits once at its end. */
	PCGEXPROPERTIES_API FEditKind ClassifyEdit(const FPropertyChangedEvent& Event, FName SchemaMember, FName RowsMember);
}
#endif
