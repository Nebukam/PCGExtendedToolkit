// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Sharing/PCGExBeacon.h"

#if WITH_EDITOR
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#endif

#include "PCGEx.h"
#include "PCGExSubSystem.h"

void UPCGExBeacon::BeginDestroy()
{
	Super::BeginDestroy();
}

void UPCGExBeacon::InternalSet(const uint32 Key, const FPCGDataCollection& InData)
{
	FWriteScopeLock WriteScopeLock(ContentLock);
	FPCGDataCollection* CollectionPtr = GetContentMap()->Find(Key);
	if (!CollectionPtr)
	{
		PCGEX_SUBSYSTEM
		PCGExSubsystem->RegisterBeacon(this);

		GetContentMap()->Set(Key, InData);
		return;
	}

	// TODO : Register this data asset to the subsystem
	// So transient data can be manually flushed when the subsystem is deactivated (PIE)

	FPCGDataCollection& Collection = *CollectionPtr;
	Collection.TaggedData.Append(Collection.TaggedData);
	//OnUpdate(InSource, Key);
}

void UPCGExBeacon::InternalAdd(const uint32 Key, const FPCGDataCollection& InData)
{
	FWriteScopeLock WriteScopeLock(ContentLock);
	FPCGDataCollection* CollectionPtr = GetContentMap()->Find(Key);
	if (!CollectionPtr)
	{
		PCGEX_SUBSYSTEM
		PCGExSubsystem->RegisterBeacon(this);

		GetContentMap()->Add(Key, InData);
		return;
	}

	// TODO : Register this data asset to the subsystem
	// So transient data can be manually flushed when the subsystem is deactivated (PIE)

	FPCGDataCollection& Collection = *CollectionPtr;
	Collection.TaggedData.Append(Collection.TaggedData);
	//OnUpdate(InSource, Key);
}

void UPCGExBeacon::InternalRemove(const uint32 Key, const FPCGDataCollection& InData)
{
	FWriteScopeLock WriteScopeLock(ContentLock);
	// TODO : Implement
	//OnUpdate(InSource, Key);
}

int32 UPCGExBeacon::Grab(const uint32 Key, FPCGDataCollection& OutData, PCGExDataSharing::FDataFilterFunc&& Filter)
{
	if (bFlushing) { return 0; }


	FReadScopeLock ReadScopeLock(ContentLock);
	FPCGDataCollection* CollectionPtr = GetContentMap()->Find(Key);

	if (!CollectionPtr) { return 0; }

	int32 AddCount = 0;
	for (const FPCGTaggedData& TaggedData : CollectionPtr->TaggedData)
	{
		if (!Filter(TaggedData)) { continue; }
		OutData.TaggedData.Emplace(TaggedData);
		AddCount++;
	}

	return AddCount;
}

int32 UPCGExBeacon::Grab(const uint32 Key, FPCGDataCollection& OutData)
{
	return Grab(Key, OutData, [](const FPCGTaggedData& InData) { return true; });
}

void UPCGExBeacon::Empty()
{
	FWriteScopeLock WriteScopeLock(ContentLock);

	if (bFlushing) { return; }

	bFlushing = true;

	GetContentMap()->Empty();

	for (const TPair<uint32, TSharedPtr<PCGExDataSharing::FDataBucket>>& Pair : PartitionedContentMap) { Pair.Value->Empty(); }
	PartitionedContentMap.Empty();

	bFlushing = false;
}

TSharedPtr<PCGExDataSharing::FDataBucket> UPCGExBeacon::GetContentMap()
{
	if (!ContentMap) { ContentMap = MakeShared<PCGExDataSharing::FDataBucket>(); }
	return ContentMap;
}

void UPCGExBeacon::EnsureSubsystemRegistration()
{
}

void UPCGExBeacon::OnUpdate(UPCGComponent* InSource, uint32 Item) const
{
	PCGEX_SUBSYSTEM
	//PCGExSubsystem->PollEvent(InSource, EPCGExSubsystemEventType::DataUpdate, HashCombineFast(BucketId, Item));
}
