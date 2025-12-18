// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExAssetCollection.h"
#include "PCGExContext.h"
#include "PCGParamData.h"
#include "Details/PCGExDetailsStaging.h"

#define PCGEX_FOREACH_COLLECTION_TYPE(MACRO, ...)\
MACRO(Mesh, __VA_ARGS__)\
MACRO(Actor, __VA_ARGS__)\
MACRO(PCGDataAsset, __VA_ARGS__)

namespace PCGExCollectionHelpers
{
	PCGEXTENDEDTOOLKIT_API
	bool BuildFromAttributeSet(UPCGExAssetCollection* InCollection, FPCGExContext* InContext, const UPCGParamData* InAttributeSet, const FPCGExAssetAttributeSetDetails& Details, const bool bBuildStaging = false);
	
	PCGEXTENDEDTOOLKIT_API
	bool BuildFromAttributeSet(UPCGExAssetCollection* InCollection, FPCGExContext* InContext, const FName InputPin, const FPCGExAssetAttributeSetDetails& Details, const bool bBuildStaging);
	
#pragma region GetEntry

	template <typename T>
	bool GetEntryAtTpl(UPCGExAssetCollection* Source, const T*& OutEntry, const TArray<T>& InEntries, const int32 Index, const UPCGExAssetCollection*& OutHost)
	{
		const int32 Pick = Source->LoadCache()->Main->GetPick(Index, EPCGExIndexPickMode::Ascending);
		if (!InEntries.IsValidIndex(Pick)) { return false; }
		OutEntry = &InEntries[Pick];
		OutHost = Source;
		return true;
	}

	template <typename T>
	bool GetEntryWeightedRandomTpl(UPCGExAssetCollection* Source, const T*& OutEntry, const TArray<T>& InEntries, const int32 Seed, const UPCGExAssetCollection*& OutHost)
	{
		const int32 PickedIndex = Source->LoadCache()->Main->GetPickRandomWeighted(Seed);
		if (!InEntries.IsValidIndex(PickedIndex)) { return false; }

		const T& Entry = InEntries[PickedIndex];
		if (Entry.bIsSubCollection && Entry.SubCollection)
		{
			return Entry.SubCollection->GetEntryWeightedRandom(OutEntry, Seed * 2, OutHost);
		}
		OutEntry = &Entry;
		OutHost = Source;
		return true;
	}
	
	template <typename T>
	bool GetEntryTpl(UPCGExAssetCollection* Source, const T*& OutEntry, const TArray<T>& InEntries, const int32 Index, const int32 Seed, const EPCGExIndexPickMode PickMode, const UPCGExAssetCollection*& OutHost)
	{
		const int32 PickedIndex = Source->LoadCache()->Main->GetPick(Index, PickMode);
		if (!InEntries.IsValidIndex(PickedIndex)) { return false; }

		if (const T& Entry = InEntries[PickedIndex]; Entry.bIsSubCollection && Entry.SubCollection)
		{
			return Entry.SubCollection->GetEntryWeightedRandom(OutEntry, Seed, OutHost);
		}
		else
		{
			OutEntry = &Entry;
			OutHost = Source;
		}
		return true;
	}

	template <typename T>
	bool GetEntryRandomTpl(UPCGExAssetCollection* Source, const T*& OutEntry, const TArray<T>& InEntries, const int32 Seed, const UPCGExAssetCollection*& OutHost)
	{
		const int32 PickedIndex = Source->LoadCache()->Main->GetPickRandom(Seed);
		if (!InEntries.IsValidIndex(PickedIndex)) { return false; }

		const T& Entry = InEntries[PickedIndex];
		if (Entry.bIsSubCollection && Entry.SubCollection)
		{
			return Entry.SubCollection->GetEntryRandom(OutEntry, Seed * 2, OutHost);
		}
		OutEntry = &Entry;
		OutHost = Source;
		return true;
	}

#pragma endregion

#pragma region GetEntryWithTags

	template <typename T>
	bool GetEntryAtTpl(UPCGExAssetCollection* Source, const T*& OutEntry, const TArray<T>& InEntries, const int32 Index, uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost)
	{
		const int32 PickedIndex = Source->LoadCache()->Main->GetPick(Index, EPCGExIndexPickMode::Ascending);
		if (!InEntries.IsValidIndex(PickedIndex)) { return false; }

		const T& Entry = InEntries[PickedIndex];

		if (Entry.bIsSubCollection && Entry.SubCollection && (TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Collection))) { OutTags.Append(Entry.SubCollection->CollectionTags); }
		if ((TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Asset))) { OutTags.Append(Entry.Tags); }

		OutEntry = &Entry;
		OutHost = Source;
		return true;
	}

	template <typename T>
	bool GetEntryTpl(UPCGExAssetCollection* Source, const T*& OutEntry, const TArray<T>& InEntries, const int32 Index, const int32 Seed, const EPCGExIndexPickMode PickMode, uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost)
	{
		const int32 PickedIndex = Source->LoadCache()->Main->GetPick(Index, PickMode);
		if (!InEntries.IsValidIndex(PickedIndex)) { return false; }

		const T& Entry = InEntries[PickedIndex];
		if (Entry.bIsSubCollection && Entry.SubCollection)
		{
			if ((TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Hierarchy))) { OutTags.Append(Entry.Tags); }
			if ((TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Collection))) { OutTags.Append(Entry.SubCollection->CollectionTags); }
			return Entry.SubCollection->GetEntryWeightedRandom(OutEntry, Seed * 2, OutHost);
		}
		if ((TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Asset))) { OutTags.Append(Entry.Tags); }
		OutEntry = &Entry;
		OutHost = Source;
		return true;
	}

	template <typename T>
	bool GetEntryRandomTpl(UPCGExAssetCollection* Source, const T*& OutEntry, const TArray<T>& InEntries, const int32 Seed, uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost)
	{
		const int32 PickedIndex = Source->LoadCache()->Main->GetPickRandom(Seed);
		if (!InEntries.IsValidIndex(PickedIndex)) { return false; }

		const T& Entry = InEntries[PickedIndex];
		if (Entry.bIsSubCollection && Entry.SubCollection)
		{
			if ((TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Hierarchy))) { OutTags.Append(Entry.Tags); }
			if ((TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Collection))) { OutTags.Append(Entry.SubCollection->CollectionTags); }
			return Entry.SubCollection->GetEntryRandom(OutEntry, Seed * 2, OutHost);
		}
		if ((TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Asset))) { OutTags.Append(Entry.Tags); }
		OutEntry = &Entry;
		OutHost = Source;
		return true;
	}

	template <typename T>
	bool GetEntryWeightedRandomTpl(UPCGExAssetCollection* Source, const T*& OutEntry, const TArray<T>& InEntries, const int32 Seed, uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost)
	{
		const int32 PickedIndex = Source->LoadCache()->Main->GetPickRandomWeighted(Seed);
		if (!InEntries.IsValidIndex(PickedIndex)) { return false; }

		const T& Entry = InEntries[PickedIndex];
		if (Entry.bIsSubCollection && Entry.SubCollection)
		{
			if ((TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Hierarchy))) { OutTags.Append(Entry.Tags); }
			if ((TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Collection))) { OutTags.Append(Entry.SubCollection->CollectionTags); }
			return Entry.SubCollection->GetEntryWeightedRandom(OutEntry, Seed * 2, OutHost);
		}
		if ((TagInheritance & static_cast<uint8>(EPCGExAssetTagInheritance::Asset))) { OutTags.Append(Entry.Tags); }
		OutEntry = &Entry;
		OutHost = Source;
		return true;
	}

#pragma endregion
}
