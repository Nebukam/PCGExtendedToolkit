// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExGridTracking.h"

#include "PCGExSubSystem.h"
#include "PCGGraph.h"


FPCGExGridID::FPCGExGridID(const FVector& InLocation, const int32 InGridSize, const FName InWorldID)
	: Name(InWorldID), GridSize(FMath::Max(1, InGridSize)), Location(InLocation)
{
}

FPCGExGridID::FPCGExGridID(const UPCGComponent* InComponent, const FVector& InLocation, const FName InName)
{
	Location = InLocation;
	Name = InName;
	if (const UPCGGraph* Graph = InComponent->GetGraph()) { GridSize = Graph->GetDefaultGridSize(); }
}

FPCGExGridID::FPCGExGridID(const UPCGComponent* InComponent, const FName InName)
{
	const AActor* Owner = InComponent->GetOwner();
	Location = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
	Name = InName;
	if (const UPCGGraph* Graph = InComponent->GetGraph()) { GridSize = Graph->GetDefaultGridSize(); }
}

FPCGExGridID FPCGExGridID::MakeFromGridID(const FVector& InLocation) const
{
	return FPCGExGridID(InLocation, GridSize, Name);
}

int32 UPCGExGridIDTracker::GetCounter(uint32 Hash) const
{
	FReadScopeLock ReadScopeLock(BucketLock);
	const int32* Count = Buckets.Find(Hash);
	return Count ? *Count : 0;
}

void UPCGExGridIDTracker::PollEvent(uint32 Hash, const int32 Diff)
{
	if (Diff == 0) { return; }

	bool bExpected = false;
	if (bIsTickScheduled.compare_exchange_strong(bExpected, true, std::memory_order_acq_rel))
	{
		// Task can be tentatively ended
		PCGEX_SUBSYSTEM
		PCGExSubsystem->RegisterBeginTickAction([this]() { ProcessPolledEvents(); });
	}

	{
		FWriteScopeLock WriteScopeLock(PollLock);
		int32& Count = PolledEvents.FindOrAdd(Hash, 0);
		Count += Diff;
	}
}

void UPCGExGridIDTracker::ProcessPolledEvents()
{
	{
		FWriteScopeLock WriteScopeLock(PollLock);
		TMap<uint32, int32> TempPolledEvents = MoveTemp(PolledEvents);
		PolledEvents.Reset();

		{
			FWriteScopeLock WriteBucketScopeLock(BucketLock);
			for (const TPair<uint32, int32>& Pair : TempPolledEvents)
			{
				if (Pair.Value == 0)
				{
					// No Meaningful change
					continue;
				}

				int32* OldCount = Buckets.Find(Pair.Key);
				if (!OldCount)
				{
					if (Pair.Value < 0)
					{
						// No Meaningful change
						continue;
					}

					Buckets.Add(Pair.Key, Pair.Value);
					OnGridIDCreated.Broadcast(Pair.Key, Pair.Value);
					OnGridIDDiff.Broadcast(Pair.Key, Pair.Value, Pair.Value);
					continue;
				}

				const int32 NewCount = *OldCount + Pair.Value;
				if (NewCount <= 0)
				{
					// Destroyed
					Buckets.Remove(Pair.Key);
					OnGridIDDiff.Broadcast(Pair.Key, NewCount, Pair.Value);
					OnGridIDDestroyed.Broadcast(Pair.Key);
				}
				else
				{
					Buckets.Add(Pair.Key, NewCount);
					OnGridIDDiff.Broadcast(Pair.Key, NewCount, Pair.Value);
				}
			}
		}
	}
}
