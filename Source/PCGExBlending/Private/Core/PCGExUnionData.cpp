// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExUnionData.h"

#include "Containers/PCGExIndexLookup.h"
#include "Data/PCGExData.h"
#include "Math/PCGExMathDistances.h"

namespace PCGExData
{
	int32 IUnionData::ComputeWeights(
		const TArray<const UPCGBasePointData*>& Sources,
		const TSharedPtr<PCGEx::FIndexLookup>& IdxLookup,
		const FPoint& Target,
		const PCGExMath::IDistances* InDistances,
		TArray<FWeightedPoint>& OutWeightedPoints) const
	{
		const int32 NumElements = Elements.Num();
		OutWeightedPoints.Reset(NumElements);

		double MaxWeight = 0;
		double TotalWeight = 0;
		int32 Index = 0;

		for (const FElement& Element : Elements)
		{
			const int32 IOIdx = IdxLookup->Get(Element.IO);
			if (IOIdx == -1) { continue; }

			FWeightedPoint& P = OutWeightedPoints.Emplace_GetRef(Element.Index, 0, IOIdx);
			const double Dist = InDistances->GetDistSquared(FConstPoint(Sources[P.IO], P), Target);
			P.Weight = Dist;

			MaxWeight = FMath::Max(MaxWeight, Dist);
			Index++;
		}

		if (Index == 0) { return 0; }

		// Normalize & one minus distances to make them weights
		for (FWeightedPoint& P : OutWeightedPoints) { TotalWeight += (P.Weight = 1 - (P.Weight / MaxWeight)); }

		if (Index == 1)
		{
			OutWeightedPoints[0].Weight = 1;
			return 1;
		}

		if (TotalWeight == 0)
		{
			const double StaticWeight = 1 / static_cast<double>(Index);
			for (FWeightedPoint& P : OutWeightedPoints) { P.Weight = StaticWeight; }
			return Index;
		}

		// Normalize weights
		//for (FWeightedPoint& P : OutWeightedPoints) { P.Weight /= TotalWeight; }
		return Index;
	}

	void IUnionData::Reserve(const int32 InSetReserve, const int32 InElementReserve)
	{
		if (InElementReserve > 8 && Elements.Max() < InElementReserve) { Elements.Reserve(InElementReserve); }
		if (InSetReserve > 8) { IOSet.Reserve(InSetReserve); }
	}

	void IUnionData::Reset()
	{
		IOSet.Reset();
		Elements.Reset();
	}

	void FUnionMetadata::SetNum(const int32 InNum)
	{
		// To be used only with NewEntryAt / NewEntryAt_Unsafe
		Entries.Init(nullptr, InNum);
	}

	FUnionMetadataBuilder::FUnionMetadataBuilder(FUnionMetadata* InTarget)
        : Target(InTarget)
    {
        // Pre-allocate thread buffers based on hardware concurrency
        const int32 NumThreads = FMath::Max(1, FPlatformMisc::NumberOfWorkerThreadsToSpawn() + 1);
        ThreadBuffers.SetNum(NumThreads);
    }

    int32 FUnionMetadataBuilder::NewEntry(const FConstPoint& Point)
    {
        FWriteScopeLock Lock(CreationLock);
        
        TSharedPtr<IUnionData> NewUnionData = Target->Entries.Add_GetRef(MakeShared<IUnionData>());
        NewUnionData->Add(Point);  // Safe - we hold the lock, no other thread can access this entry yet
        
        return Target->Entries.Num() - 1;
    }

    void FUnionMetadataBuilder::Append(int32 EntryIndex, const FPoint& Point)
    {
        // Get thread-local buffer (lock-free path for common case)
        const int32 ThreadId = FPlatformTLS::GetCurrentThreadId() % ThreadBuffers.Num();
        ThreadBuffers[ThreadId].Add({EntryIndex, FElement(Point.Index, Point.IO)});
    }

    void FUnionMetadataBuilder::Finalize()
    {
        // Merge all thread buffers into target entries
        // This is single-threaded, so no locks needed
        for (TArray<FPendingAppend>& Buffer : ThreadBuffers)
        {
            for (const FPendingAppend& Pending : Buffer)
            {
                if (Target->Entries.IsValidIndex(Pending.EntryIndex))
                {
                    Target->Entries[Pending.EntryIndex]->Add(Pending.Element);
                }
            }
            Buffer.Reset();
        }
    }

    void FUnionMetadata::BeginConcurrentBuild(int32 ReserveCount)
    {
        check(!Builder.IsValid());
        Builder = MakeUnique<FUnionMetadataBuilder>(this);
        if (ReserveCount > 0) { Entries.Reserve(ReserveCount); }
    }

    void FUnionMetadata::EndConcurrentBuild()
    {
        if (Builder)
        {
            Builder->Finalize();
            Builder.Reset();
        }
    }

    int32 FUnionMetadata::NewEntry_Unsafe(const FConstPoint& Point)
    {
        TSharedPtr<IUnionData> NewUnionData = Entries.Add_GetRef(MakeShared<IUnionData>());
        NewUnionData->Add(Point);
        return Entries.Num() - 1;
    }

	TSharedPtr<IUnionData> FUnionMetadata::NewEntryAt_Unsafe(const int32 ItemIndex)
	{
		Entries[ItemIndex] = MakeShared<IUnionData>();
		return Entries[ItemIndex];
	}

	bool FUnionMetadata::IOIndexOverlap(const int32 InIdx, const TSet<int32>& InIndices)
	{
		const TSet<int32> Overlap = Entries[InIdx]->IOSet.Intersect(InIndices);
		return Overlap.Num() > 0;
	}
}
