// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "AssetStaging/PCGExMeshCollectionToGrammar.h"


#include "PCGGraph.h"
#include "PCGPin.h"
#include "AssetStaging/PCGExStaging.h"
#include "Collections/PCGExActorCollection.h"
#include "Collections/PCGExMeshCollection.h"
#include "Elements/Grammar/PCGSubdivisionBase.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"
#define PCGEX_NAMESPACE MeshCollectionToGrammar

#pragma region UPCGSettings interface

TArray<FPCGPinProperties> UPCGExMeshCollectionToGrammarSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExMeshCollectionToGrammarSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_PARAM(FName("ModuleInfos"), TEXT("Module infos generated from the selected collection"), Normal)
	PCGEX_PIN_PARAM(PCGExStaging::OutputCollectionMapLabel, "Collection map", Normal)
	return PinProperties;
}

FPCGElementPtr UPCGExMeshCollectionToGrammarSettings::CreateElement() const { return MakeShared<FPCGExMeshCollectionToGrammarElement>(); }

#pragma endregion

#define PCGEX_FOREACH_MODULE_FIELD(MACRO)\
MACRO(Symbol, FName, Infos.Symbol, NAME_None)\
MACRO(Size, double, Infos.Size, 0)\
MACRO(Scalable, bool, Infos.bScalable, true)\
MACRO(DebugColor, FVector4, Infos.DebugColor, FVector4(1,1,1,1))\
MACRO(Entry, int64, Idx, 0)\
MACRO(Category, FName, Entry->Category, NAME_None)

bool FPCGExMeshCollectionToGrammarElement::IsCacheable(const UPCGSettings* InSettings) const
{
	const UPCGExMeshCollectionToGrammarSettings* Settings = static_cast<const UPCGExMeshCollectionToGrammarSettings*>(InSettings);
	PCGEX_GET_OPTION_STATE(Settings->CacheData, bDefaultCacheNodeOutput)
}

bool FPCGExMeshCollectionToGrammarElement::ExecuteInternal(FPCGContext* Context) const
{
	PCGEX_SETTINGS(MeshCollectionToGrammar)

	UPCGExMeshCollection* MainCollection = PCGExHelpers::LoadBlocking_AnyThread(Settings->MeshCollection);

	if (!MainCollection)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Mesh collection failed to load."));
		return true;
	}

	TSharedPtr<PCGExStaging::FPickPacker> Packer = MakeShared<PCGExStaging::FPickPacker>(Context);

	MainCollection->EDITOR_RegisterTrackingKeys(static_cast<FPCGExContext*>(Context));

	UPCGParamData* OutputModules = NewObject<UPCGParamData>();
	UPCGMetadata* Metadata = OutputModules->Metadata;

#define PCGEX_DECLARE_ATT(_NAME, _TYPE, _GETTER, _DEFAULT) \
	FPCGMetadataAttribute<_TYPE>* _NAME##Attribute = nullptr; \
	PCGEX_VALIDATE_NAME(Settings->_NAME##AttributeName) _NAME##Attribute = Metadata->FindOrCreateAttribute<_TYPE>(PCGEx::GetAttributeIdentifier(Settings->_NAME##AttributeName, OutputModules), _DEFAULT, false, true);
	PCGEX_FOREACH_MODULE_FIELD(PCGEX_DECLARE_ATT);
#undef PCGEX_DECLARE_ATT

	TSet<FName> UniqueSymbols;
	UniqueSymbols.Reserve(100);
	
	TMap<const FPCGExMeshCollectionEntry*, double> SizeCache;
	SizeCache.Reserve(100);

	TArray<PCGExMeshCollectionToGrammar::FModule> Modules;
	Modules.Reserve(100);

	FlattenCollection(Packer, MainCollection, Settings, Modules, UniqueSymbols, SizeCache);

	for (const PCGExMeshCollectionToGrammar::FModule& Module : Modules)
	{
		const int64 Key = Metadata->AddEntry();
#define PCGEX_MODULE_OUT(_NAME, _TYPE, _GETTER, _DEFAULT) _NAME##Attribute->SetValue(Key, Module._GETTER);
		PCGEX_FOREACH_MODULE_FIELD(PCGEX_MODULE_OUT)
#undef PCGEX_MODULE_OUT
	}

	FPCGTaggedData& ModulesData = Context->OutputData.TaggedData.Emplace_GetRef();
	ModulesData.Pin = FName("ModuleInfos");
	ModulesData.Data = OutputModules;

	UPCGParamData* OutputMap = NewObject<UPCGParamData>();
	Packer->PackToDataset(OutputMap);

	FPCGTaggedData& CollectionMapData = Context->OutputData.TaggedData.Emplace_GetRef();
	CollectionMapData.Pin = PCGExStaging::OutputCollectionMapLabel;
	CollectionMapData.Data = OutputMap;

	return true;
}

void FPCGExMeshCollectionToGrammarElement::FlattenCollection(
	const TSharedPtr<PCGExStaging::FPickPacker>& Packer,
	UPCGExMeshCollection* Collection,
	const UPCGExMeshCollectionToGrammarSettings* Settings,
	TArray<PCGExMeshCollectionToGrammar::FModule>& OutModules,
	TSet<FName>& UniqueSymbols,
	TMap<const FPCGExMeshCollectionEntry*, double>& SizeCache) const
{
	if (!Collection) { return; }

	const PCGExAssetCollection::FCache* Cache = Collection->LoadCache();
	const int32 NumEntries = Cache->Main->Order.Num();

	const FPCGExMeshCollectionEntry* Entry = nullptr;
	const UPCGExAssetCollection* EntryHost = nullptr;

	for (int i = 0; i < NumEntries; i++)
	{
		Collection->GetEntryAt(Entry, i, EntryHost);
		if (!Entry) { continue; }

		if (Entry->bIsSubCollection && Entry->SubGrammarMode == EPCGExGrammarSubCollectionMode::Flatten)
		{
			FlattenCollection(Packer, Entry->SubCollection, Settings, OutModules, UniqueSymbols, SizeCache);
			continue;
		}

		PCGExMeshCollectionToGrammar::FModule& Module = OutModules.Emplace_GetRef();
		if (!Entry->FixModuleInfos(Collection, Module.Infos)
			|| (Settings->bSkipEmptySymbol && Module.Infos.Symbol.IsNone()) )
		{
			OutModules.Pop(EAllowShrinking::No);
			continue;
		}

		bool bIsAlreadyInSet = false;
		UniqueSymbols.Add(Module.Infos.Symbol, &bIsAlreadyInSet);

		if (bIsAlreadyInSet && !Settings->bAllowDuplicates)
		{
			OutModules.Pop(EAllowShrinking::No);
			continue;
		}

		Module.Entry = Entry;
		Module.Idx = Packer->GetPickIdx(EntryHost, Entry->Staging.InternalIndex, 0);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
#undef PCGEX_FOREACH_MODULE_FIELD
