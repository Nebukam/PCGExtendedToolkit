// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "PCGExStaging.h"
#include "Elements/Grammar/PCGSubdivisionBase.h"
#include "PCGExMeshCollectionToGrammar.generated.h"

class UPCGExMeshCollection;

namespace PCGExMeshCollectionToGrammar
{
	struct FModule
	{
		FModule() = default;
		FPCGSubdivisionSubmodule Infos;
		int64 Idx;
	};
}

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), meta=(PCGExNodeLibraryDoc="assets-management/asset-collection-to-set"))
class UPCGExMeshCollectionToGrammarSettings : public UPCGSettings
{
	GENERATED_BODY()

	friend class FPCGExMeshCollectionToGrammarElement;

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_DUMMY_SETTINGS_MEMBERS
	PCGEX_NODE_INFOS(MeshCollectionToGrammar, "Collection to Module Infos", "Converts a mesh collection to a grammar-friendly attribute set that can be used as module infos.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	/** The mesh collection to read module infos from */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TSoftObjectPtr<UPCGExMeshCollection> MeshCollection;

	/** If enabled, allows duplicate entries (duplicate is same symbol) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bAllowDuplicates = true;

	/** If enabled, invalid or empty entries are removed from the output */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bOmitInvalidAndEmpty = true;

	/** Name of the attribute the "Symbol" value will be written to */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName="Symbol"))
	FName SymbolAttributeName = FName("Symbol");

	/** Name of the attribute the "Size" value will be written to */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName="Size"))
	FName SizeAttributeName = FName("Size");

	/** Name of the attribute the "Scalable" value will be written to */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName="Scalable"))
	FName ScalableAttributeName = FName("Scalable");

	/** Name of the attribute the "DebugColor" value will be written to */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName="DebugColor"))
	FName DebugColorAttributeName = FName("DebugColor");

	/** Mesh collection entry Idx. Serialize the id of the parent collection (in the collection map) and entry index within that collection.\nThis is a critical piece of data that will be used by the Grammar Staging node to retrieve the corresponding mesh. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName="Entry", EditCondition="false"))
	FName EntryAttributeName = PCGExStaging::Tag_EntryIdx;

	/** Cache the results of this node. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Performance, meta=(PCG_NotOverridable))
	EPCGExOptionState CacheData = EPCGExOptionState::Default;
};

class FPCGExMeshCollectionToGrammarElement final : public IPCGElement
{
public:
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override;

protected:
	PCGEX_ELEMENT_CREATE_DEFAULT_CONTEXT

	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

	void FlattenCollection(
		const TSharedPtr<PCGExStaging::FPickPacker>& Packer,
		UPCGExMeshCollection* Collection,
		const UPCGExMeshCollectionToGrammarSettings* Settings,
		TArray<PCGExMeshCollectionToGrammar::FModule>& OutModules,
		TSet<FName>& OutSymbols,
		TMap<const FPCGExMeshCollectionEntry*, double>& SizeCache) const;
};
