// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "AssetStaging/PCGExCollectionToModuleInfos.h"


#include "PCGGraph.h"
#include "PCGPin.h"
#include "AssetStaging/PCGExStaging.h"
#include "Collections/PCGExActorCollection.h"
#include "Collections/PCGExAssetCollection.h"
#include "Elements/Grammar/PCGSubdivisionBase.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"
#define PCGEX_NAMESPACE CollectionToModuleInfos

#pragma region UPCGSettings interface

TArray<FPCGPinProperties> UPCGExCollectionToModuleInfosSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExCollectionToModuleInfosSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_PARAM(FName("ModuleInfos"), TEXT("Module infos generated from the selected collection"), Normal)
	PCGEX_PIN_PARAM(PCGExStaging::OutputCollectionMapLabel, "Collection map", Normal)
	return PinProperties;
}

FPCGElementPtr UPCGExCollectionToModuleInfosSettings::CreateElement() const { return MakeShared<FPCGExCollectionToModuleInfosElement>(); }

#pragma endregion

#define PCGEX_FOREACH_MODULE_FIELD(MACRO)\
MACRO(Symbol, FName, Infos.Symbol, NAME_None)\
MACRO(Size, double, Infos.Size, 0)\
MACRO(Scalable, bool, Infos.bScalable, true)\
MACRO(DebugColor, FVector4, Infos.DebugColor, FVector4(1,1,1,1))\
MACRO(Entry, int64, Idx, 0)\
MACRO(Category, FName, Entry->Category, NAME_None)

bool FPCGExCollectionToModuleInfosElement::IsCacheable(const UPCGSettings* InSettings) const
{
	const UPCGExCollectionToModuleInfosSettings* Settings = static_cast<const UPCGExCollectionToModuleInfosSettings*>(InSettings);
	PCGEX_GET_OPTION_STATE(Settings->CacheData, bDefaultCacheNodeOutput)
}

bool FPCGExCollectionToModuleInfosElement::ExecuteInternal(FPCGContext* Context) const
{
	PCGEX_SETTINGS(CollectionToModuleInfos)

	UPCGExAssetCollection* MainCollection = PCGExHelpers::LoadBlocking_AnyThread(Settings->AssetCollection);

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

	TMap<const FPCGExAssetCollectionEntry*, double> SizeCache;
	SizeCache.Reserve(100);

	TArray<PCGExCollectionToGrammar::FModule> Modules;
	Modules.Reserve(100);

	FlattenCollection(Packer, MainCollection, Settings, Modules, UniqueSymbols, SizeCache);

	for (const PCGExCollectionToGrammar::FModule& Module : Modules)
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

void FPCGExCollectionToModuleInfosElement::FlattenCollection(
	const TSharedPtr<PCGExStaging::FPickPacker>& Packer,
	UPCGExAssetCollection* Collection,
	const UPCGExCollectionToModuleInfosSettings* Settings,
	TArray<PCGExCollectionToGrammar::FModule>& OutModules,
	TSet<FName>& UniqueSymbols,
	TMap<const FPCGExAssetCollectionEntry*, double>& SizeCache) const
{
	if (!Collection) { return; }

	const PCGExAssetCollection::FCache* Cache = Collection->LoadCache();
	const int32 NumEntries = Cache->Main->Order.Num();

	const FPCGExAssetCollectionEntry* Entry = nullptr;
	const UPCGExAssetCollection* EntryHost = nullptr;

	for (int i = 0; i < NumEntries; i++)
	{
		Collection->GetEntryAt(Entry, i, EntryHost);
		if (!Entry) { continue; }

		if (Entry->bIsSubCollection && Entry->SubGrammarMode == EPCGExGrammarSubCollectionMode::Flatten)
		{
			FlattenCollection(Packer, Entry->GetSubCollection<UPCGExAssetCollection>(), Settings, OutModules, UniqueSymbols, SizeCache);
			continue;
		}

		PCGExCollectionToGrammar::FModule& Module = OutModules.Emplace_GetRef();
		if (!Entry->FixModuleInfos(Collection, Module.Infos)
			|| (Settings->bSkipEmptySymbol && Module.Infos.Symbol.IsNone()))
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
