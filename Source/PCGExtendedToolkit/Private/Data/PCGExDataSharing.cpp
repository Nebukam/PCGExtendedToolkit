// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/PCGExDataSharing.h"

#include "PCGExSubSystem.h"

void UPCGExDataBucket::Append(UPCGComponent* InSource, const uint32 Item, const FPCGDataCollection& InData)
{
	FWriteScopeLock WriteScopeLock(ContentLock);
	FPCGDataCollection* CollectionPtr = Content.Find(Item);
	if (!CollectionPtr)
	{
		Content.Add(Item, InData);
		return;
	}

	FPCGDataCollection& Collection = *CollectionPtr;
	Collection.TaggedData.Append(Collection.TaggedData);
	OnUpdate(InSource, Item);
}

void UPCGExDataBucket::Remove(UPCGComponent* InSource, const uint32 Item, const FPCGDataCollection& InData)
{
	FWriteScopeLock WriteScopeLock(ContentLock);
	// TODO : Implement
	OnUpdate(InSource, Item);
}

void UPCGExDataBucket::Replace(UPCGComponent* InSource, const uint32 Item, const FPCGDataCollection& InData)
{
	FWriteScopeLock WriteScopeLock(ContentLock);
	Content.Add(Item, InData);
	OnUpdate(InSource, Item);
}

int32 UPCGExDataBucket::Grab(const uint32 Item, FPCGDataCollection& OutData, FDataFilterFunc&& Filter)
{
	if (bFlushing) { return 0; }

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

void UPCGExDataBucket::OnUpdate(UPCGComponent* InSource, uint32 Item) const
{
	PCGEX_SUBSYSTEM
	PCGExSubsystem->PollEvent(InSource, EPCGExSubsystemEventType::DataUpdate, HashCombineFast(BucketId, Item));
}

void UPCGExSharedDataManager::PushData(UPCGComponent* InSource, uint32 BucketId, uint32 ItemId, const FPCGDataCollection& InCollection, EPCGExDataSharingPushType InPushType)
{
	TObjectPtr<UPCGExDataBucket> Bucket = FindBucket(BucketId);
	if (!Bucket.Get())
	{
		FWriteScopeLock WriteScopeLock(BucketLock);

		{
			FGCScopeGuard Scope;
			Bucket = NewObject<UPCGExDataBucket>();
		}

		Bucket->BucketId = BucketId;
		if (Bucket->HasAnyInternalFlags(EInternalObjectFlags::Async)) { Bucket->ClearInternalFlags(EInternalObjectFlags::Async); }

		Buckets.Add(BucketId, Bucket);
	}

	switch (InPushType)
	{
	default:
	case EPCGExDataSharingPushType::Replace:
		Bucket->Replace(InSource, ItemId, InCollection);
		break;
	case EPCGExDataSharingPushType::Append:
		Bucket->Append(InSource, ItemId, InCollection);
		break;
	case EPCGExDataSharingPushType::Remove:
		Bucket->Remove(InSource, ItemId, InCollection);
		break;
	}
}

TObjectPtr<UPCGExDataBucket> UPCGExSharedDataManager::FindBucket(const uint32 BucketId)
{
	FReadScopeLock ReadScopeLock(BucketLock);

	TObjectPtr<UPCGExDataBucket>* BucketPtr = Buckets.Find(BucketId);
	if (!BucketPtr) { return nullptr; }
	return *BucketPtr;
}

void UPCGExSharedDataManager::FlushBucket(uint32 BucketId)
{
	FWriteScopeLock WriteScopeLock(BucketLock);
	TObjectPtr<UPCGExDataBucket> Bucket = FindBucket(BucketId);
	if (Bucket.Get()) { Bucket->Flush(); }
}

void UPCGExSharedDataManager::Flush()
{
	FWriteScopeLock WriteScopeLock(BucketLock);

	TArray<TObjectPtr<UPCGExDataBucket>> BucketArray;
	BucketArray.Reserve(Buckets.Num());

	for (const TPair<uint32, TObjectPtr<UPCGExDataBucket>>& Pair : Buckets) { BucketArray.Add(Pair.Value); }

	Buckets.Empty();

	for (const TObjectPtr<UPCGExDataBucket>& Bucket : BucketArray) { Bucket->Flush(); }
}
