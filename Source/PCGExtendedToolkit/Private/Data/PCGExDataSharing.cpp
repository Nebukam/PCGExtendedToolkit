// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/PCGExDataSharing.h"

void UPCGExDataBucket::Append(uint32 Item, const FPCGDataCollection& Data)
{
	FWriteScopeLock WriteScopeLock(ContentLock);
	FPCGDataCollection* CollectionPtr = Content.Find(Item);
	if (!CollectionPtr)
	{
		Content.Add(Item, Data);
		return;
	}

	FPCGDataCollection& Collection = *CollectionPtr;
	Collection.TaggedData.Append(Collection.TaggedData);
}

void UPCGExDataBucket::Remove(uint32 Item, const FPCGDataCollection& Data)
{
	FWriteScopeLock WriteScopeLock(ContentLock);
	// TODO : Implement
}

void UPCGExDataBucket::Replace(uint32 Item, const FPCGDataCollection& Data)
{
	FWriteScopeLock WriteScopeLock(ContentLock);
	// TODO : Implement
}

int32 UPCGExDataBucket::Grab(const uint32 Item, FPCGDataCollection& OutData, FDataFilterFunc&& Filter)
{
	FReadScopeLock ReadScopeLock(ContentLock);
	FPCGDataCollection* CollectionPtr = Content.Find(Item);

	if (!CollectionPtr) { return 0; }

	int32 AddCount = 0;
	for (const FPCGTaggedData& TaggedData : CollectionPtr->TaggedData)
	{
		if (!Filter(TaggedData)) { continue; }
		OutData.TaggedData.Add(TaggedData);
		AddCount++;
	}

	return AddCount;
}

void UPCGExDataBucket::Flush()
{
	FWriteScopeLock WriteScopeLock(ContentLock);

	if (bFlushing) { return; }

	bFlushing = true;

	for (const TPair<uint32, FPCGDataCollection>& Pair : Content)
	{
		// TODO : Implement
	}

	Content.Empty();

	bFlushing = false;
}

void UPCGExSharedDataManager::PushData(uint32 BucketId, uint32 ItemId, const FPCGDataCollection& InCollection, EPCGExDataSharingPushType InPushType)
{
}

UPCGExDataBucket* UPCGExSharedDataManager::FindBucket(const uint32 BucketId)
{
	FReadScopeLock ReadScopeLock(BucketLock);

	TObjectPtr<UPCGExDataBucket>* BucketPtr = Buckets.Find(BucketId);
	if (!BucketPtr) { return nullptr; }
	return *BucketPtr;
}

void UPCGExSharedDataManager::FlushBucket(uint32 BucketId)
{
}

void UPCGExSharedDataManager::Flush()
{
}
