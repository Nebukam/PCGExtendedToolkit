// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "UObject/Object.h"
#include "Data/PCGExPointElements.h"

namespace PCGExMath
{
	class IDistances;
}

namespace PCGEx
{
	class FIndexLookup;
}

class UPCGBasePointData;

namespace PCGExDetails
{
	class FDistances;
}

namespace PCGExData
{
	struct FConstPoint;
	struct FWeightedPoint;
	struct FPoint;
	struct FElement;

#pragma region Compound

	class PCGEXBLENDING_API IUnionData : public TSharedFromThis<IUnionData>
	{
	public:
		TSet<int32, DefaultKeyFuncs<int32>, InlineSparseAllocator> IOSet;
		TArray<FElement, TInlineAllocator<8>> Elements;

		IUnionData() = default;
		virtual ~IUnionData() = default;

		FORCEINLINE int32 Num() const { return Elements.Num(); }
		FORCEINLINE bool IsEmpty() const { return Elements.IsEmpty(); }

		// All Add methods are now inherently unsafe - caller ensures thread safety
		FORCEINLINE void Add(const FElement& Point)
		{
			IOSet.Add(Point.IO);
			Elements.Emplace(Point.Index == -1 ? 0 : Point.Index, Point.IO);
		}

		FORCEINLINE void Add(const int32 Index, const int32 IO)
		{
			IOSet.Add(IO);
			Elements.Emplace(Index == -1 ? 0 : Index, IO);
		}

		void Add(const int32 IOIndex, const TArray<int32>& PointIndices)
		{
			IOSet.Add(IOIndex);
			Elements.Reserve(Elements.Num() + PointIndices.Num());
			for (const int32 A : PointIndices) { Elements.Add(FElement(A, IOIndex)); }
		}

		// Polymorphic - subclasses can override for specialized weight computation
		virtual int32 ComputeWeights(
			const TArray<const UPCGBasePointData*>& Sources,
			const TSharedPtr<PCGEx::FIndexLookup>& IdxLookup,
			const FPoint& Target,
			const PCGExMath::IDistances* InDistances,
			TArray<FWeightedPoint>& OutWeightedPoints) const;

		virtual void Reserve(const int32 InSetReserve, const int32 InElementReserve = 8);
		virtual void Reset();
	};

	// Forward declare
    class FUnionMetadata;

    // Thread-safe builder for concurrent union construction
    class PCGEXBLENDING_API FUnionMetadataBuilder
    {
        friend class FUnionMetadata;

        struct FPendingAppend
        {
            int32 EntryIndex;
            FElement Element;
        };

        // Per-thread staging buffers to avoid contention
        TArray<TArray<FPendingAppend>> ThreadBuffers;
        
        // Entry creation lock (only needed when creating new entries)
        mutable FRWLock CreationLock;
        
        // Target metadata being built
        FUnionMetadata* Target = nullptr;

    public:
        explicit FUnionMetadataBuilder(FUnionMetadata* InTarget);
        ~FUnionMetadataBuilder() = default;

        // Thread-safe: Creates new entry, returns index
        int32 NewEntry(const FConstPoint& Point);
        
        // Thread-safe: Stages an append for later merge
        void Append(int32 EntryIndex, const FPoint& Point);
        
        // Single-threaded: Merges all staged appends into target
        void Finalize();
    };

    class PCGEXBLENDING_API FUnionMetadata : public TSharedFromThis<FUnionMetadata>
    {
        friend class FUnionMetadataBuilder;
        
    public:
        TArray<TSharedPtr<IUnionData>> Entries;
        bool bIsAbstract = false;

        // Optional builder for concurrent construction
        TUniquePtr<FUnionMetadataBuilder> Builder;

        FUnionMetadata() = default;
        ~FUnionMetadata() = default;

        int32 Num() const { return Entries.Num(); }
        void SetNum(const int32 InNum);

        // === Concurrent build mode ===
        void BeginConcurrentBuild(int32 ReserveCount = 0);
        void EndConcurrentBuild();
        
        FORCEINLINE bool IsBuildingConcurrently() const { return Builder.IsValid(); }
        
        // Thread-safe when Builder is active
        FORCEINLINE int32 NewEntry(const FConstPoint& Point)
        {
            return Builder ? Builder->NewEntry(Point) : NewEntry_Unsafe(Point);
        }
        
        FORCEINLINE void Append(int32 Index, const FPoint& Point)
        {
            if (Builder) { Builder->Append(Index, Point); }
            else { Entries[Index]->Add(Point); }
        }

        // === Single-threaded mode (legacy compatibility) ===
        int32 NewEntry_Unsafe(const FConstPoint& Point);
        TSharedPtr<IUnionData> NewEntryAt_Unsafe(int32 ItemIndex);
        
        FORCEINLINE void Append_Unsafe(int32 Index, const FPoint& Point) 
        { 
            Entries[Index]->Add(Point); 
        }

        // === Read-only access (always safe) ===
        bool IOIndexOverlap(int32 InIdx, const TSet<int32>& InIndices);
        
        FORCEINLINE TSharedPtr<IUnionData> Get(int32 Index) const
        {
            return Entries.IsValidIndex(Index) ? Entries[Index] : nullptr;
        }
    };

#pragma endregion
}
