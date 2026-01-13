// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExUnionData.h"

#include "Containers/PCGExIndexLookup.h"
#include "Data/PCGExData.h"
#include "Math/PCGExMathDistances.h"

namespace PCGExData
{
#pragma region IUnionData

	void IUnionData::Add(const int32 IOIndex, const TArray<int32>& PointIndices)
	{
		IOSet.Add(IOIndex);
		Elements.Reserve(Elements.Num() + PointIndices.Num());
		for (const int32 A : PointIndices) { Elements.Add(FElement(A, IOIndex)); }
	}

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

		// Weights are already computed, normalization can happen at blend time if needed
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

#pragma endregion

#pragma region FUnionMetadataBuilder

	FUnionMetadataBuilder::FUnionMetadataBuilder(FUnionMetadata* InTarget)
		: Target(InTarget)
	{
		// Use a reasonable number of buffers - more buffers = less contention but more memory
		// Cap at 32 to avoid excessive memory usage
		NumThreadBuffers = FMath::Clamp(FPlatformMisc::NumberOfWorkerThreadsToSpawn() + 1, 1, 32);
		ThreadBuffers.SetNum(NumThreadBuffers);

		// Create per-buffer locks
		BufferLocks.SetNum(NumThreadBuffers);
		for (int32 i = 0; i < NumThreadBuffers; ++i)
		{
			BufferLocks[i] = MakeUnique<FCriticalSection>();
		}
	}

	int32 FUnionMetadataBuilder::NewEntry(const FConstPoint& Point)
	{
		FWriteScopeLock Lock(CreationLock);

		TSharedPtr<IUnionData> NewUnionData = Target->Entries.Add_GetRef(MakeShared<IUnionData>());
		// Safe to add directly here - we hold the creation lock and no other thread
		// can access this entry until we release the lock and return the index
		NewUnionData->Add(Point);

		return Target->Entries.Num() - 1;
	}

	void FUnionMetadataBuilder::Append(const int32 EntryIndex, const FPoint& Point)
	{
		// Distribute across buffers using thread ID
		const uint32 ThreadId = FPlatformTLS::GetCurrentThreadId();
		const int32 BufferIndex = static_cast<int32>(ThreadId % NumThreadBuffers);

		// Lock the specific buffer - threads colliding on same buffer will serialize
		FScopeLock Lock(BufferLocks[BufferIndex].Get());
		ThreadBuffers[BufferIndex].Emplace(EntryIndex, FElement(Point.Index, Point.IO));
	}

	void FUnionMetadataBuilder::Append(const int32 EntryIndex, const FConstPoint& Point)
	{
		// Distribute across buffers using thread ID
		const uint32 ThreadId = FPlatformTLS::GetCurrentThreadId();
		const int32 BufferIndex = static_cast<int32>(ThreadId % NumThreadBuffers);

		// Lock the specific buffer - threads colliding on same buffer will serialize
		FScopeLock Lock(BufferLocks[BufferIndex].Get());
		ThreadBuffers[BufferIndex].Emplace(EntryIndex, FElement(Point.Index, Point.IO));
	}

	void FUnionMetadataBuilder::Finalize()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FUnionMetadataBuilder::Finalize);

		// Merge all thread buffers into target entries
		// This runs single-threaded after all concurrent operations complete
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

	void FUnionMetadataBuilder::Reserve(const int32 ExpectedAppendsPerThread)
	{
		for (TArray<FPendingAppend>& Buffer : ThreadBuffers)
		{
			Buffer.Reserve(FMath::Max(64, ExpectedAppendsPerThread));
		}
	}

#pragma endregion

#pragma region FUnionMetadata

	void FUnionMetadata::SetNum(const int32 InNum)
	{
		// To be used only with NewEntryAt_Unsafe pattern
		Entries.Init(nullptr, InNum);
	}

	void FUnionMetadata::BeginConcurrentBuild(const int32 ReserveCount)
	{
		check(!Builder.IsValid()); // Don't begin twice
		Builder = MakeUnique<FUnionMetadataBuilder>(this);

		if (ReserveCount > 0)
		{
			Entries.Reserve(ReserveCount);
			// Estimate average of 2-4 appends per entry for thread buffer sizing
			Builder->Reserve(FMath::Max(64, ReserveCount / Builder->NumThreadBuffers));
		}
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

	bool FUnionMetadata::IOIndexOverlap(const int32 InIdx, const TSet<int32>& InIndices) const
	{
		const TSet<int32> Overlap = Entries[InIdx]->IOSet.Intersect(InIndices);
		return Overlap.Num() > 0;
	}

#pragma endregion
}