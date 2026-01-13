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

#pragma region Union Data

	/**
	 * Lock-free union data container.
	 * Stores element references from multiple sources that have been fused together.
	 * 
	 * THREAD SAFETY:
	 * - All mutation methods are inherently UNSAFE - caller must ensure thread safety
	 * - Read methods (Num, IsEmpty, ComputeWeights) are always safe after build phase
	 * - Use FUnionMetadataBuilder for thread-safe construction
	 * 
	 * INHERITANCE:
	 * - Subclass and override ComputeWeights() for specialized weight computation
	 * - See FSamplingUnionData for an example
	 */
	class PCGEXBLENDING_API IUnionData : public TSharedFromThis<IUnionData>
	{
	public:
		/** Set of unique IO indices that contributed to this union */
		TSet<int32, DefaultKeyFuncs<int32>, InlineSparseAllocator> IOSet;

		/** All elements in this union (may have multiple from same IO) */
		TArray<FElement, TInlineAllocator<8>> Elements;

		IUnionData() = default;
		virtual ~IUnionData() = default;

		/** Returns the number of elements in this union */
		FORCEINLINE int32 Num() const { return Elements.Num(); }

		/** Returns true if no elements have been added */
		FORCEINLINE bool IsEmpty() const { return Elements.IsEmpty(); }

		/**
		 * Add an element to this union.
		 * NOT THREAD SAFE - caller must ensure synchronization.
		 */
		FORCEINLINE void Add(const FElement& Point)
		{
			IOSet.Add(Point.IO);
			Elements.Emplace(Point.Index == -1 ? 0 : Point.Index, Point.IO);
		}

		/**
		 * Add an element by index and IO.
		 * NOT THREAD SAFE - caller must ensure synchronization.
		 */
		FORCEINLINE void Add(const int32 Index, const int32 IO)
		{
			IOSet.Add(IO);
			Elements.Emplace(Index == -1 ? 0 : Index, IO);
		}

		/**
		 * Add multiple point indices from a single IO source.
		 * NOT THREAD SAFE - caller must ensure synchronization.
		 */
		void Add(int32 IOIndex, const TArray<int32>& PointIndices);

		/**
		 * Compute blending weights for all elements in this union.
		 * Override in subclasses for specialized weight computation (e.g., pre-computed weights).
		 * 
		 * @param Sources Array of source point data objects
		 * @param IdxLookup Lookup table mapping IO indices to source array indices
		 * @param Target The target point being blended to
		 * @param InDistances Distance calculation strategy
		 * @param OutWeightedPoints Output array of weighted point references
		 * @return Number of valid weighted points produced
		 */
		virtual int32 ComputeWeights(
			const TArray<const UPCGBasePointData*>& Sources,
			const TSharedPtr<PCGEx::FIndexLookup>& IdxLookup,
			const FPoint& Target,
			const PCGExMath::IDistances* InDistances,
			TArray<FWeightedPoint>& OutWeightedPoints) const;

		/**
		 * Pre-allocate memory for expected element count.
		 * NOT THREAD SAFE - call before concurrent access begins.
		 */
		virtual void Reserve(int32 InSetReserve, int32 InElementReserve = 8);

		/**
		 * Clear all elements and reset state for reuse.
		 * NOT THREAD SAFE - ensure no concurrent access.
		 */
		virtual void Reset();
	};

#pragma endregion

#pragma region Union Metadata Builder

	class FUnionMetadata;

	/**
	 * Thread-safe builder for concurrent union metadata construction.
	 * 
	 * Uses per-thread staging buffers with per-buffer locks to handle hash collisions.
	 * Call Finalize() after all concurrent insertions complete to merge staged data.
	 * 
	 * USAGE:
	 *   metadata->BeginConcurrentBuild(reserveCount);
	 *   // ... parallel insertions via metadata->NewEntry() and metadata->Append() ...
	 *   metadata->EndConcurrentBuild();  // Calls Finalize() and releases builder
	 */
	class PCGEXBLENDING_API FUnionMetadataBuilder
	{
		friend class FUnionMetadata;

	public:
		/** Staged append operation - (EntryIndex, Element) pair */
		struct FPendingAppend
		{
			int32 EntryIndex;
			FElement Element;

			FPendingAppend() = default;
			FPendingAppend(const int32 InEntryIndex, const FElement& InElement)
				: EntryIndex(InEntryIndex), Element(InElement)
			{
			}
		};

	private:
		/** Per-thread staging buffers to minimize contention */
		TArray<TArray<FPendingAppend>> ThreadBuffers;

		/** Per-buffer locks - threads may collide on same buffer index */
		TArray<TUniquePtr<FCriticalSection>> BufferLocks;

		/** Lock for entry creation (only acquired when creating NEW entries) */
		mutable FRWLock CreationLock;

		/** Target metadata being built */
		FUnionMetadata* Target = nullptr;

		/** Number of buffers for distribution */
		int32 NumThreadBuffers = 1;

	public:
		explicit FUnionMetadataBuilder(FUnionMetadata* InTarget);
		~FUnionMetadataBuilder() = default;

		// Non-copyable, non-movable
		FUnionMetadataBuilder(const FUnionMetadataBuilder&) = delete;
		FUnionMetadataBuilder& operator=(const FUnionMetadataBuilder&) = delete;
		FUnionMetadataBuilder(FUnionMetadataBuilder&&) = delete;
		FUnionMetadataBuilder& operator=(FUnionMetadataBuilder&&) = delete;

		/**
		 * Create a new union entry. Thread-safe.
		 * @param Point Initial point to add to the new entry
		 * @return Index of the newly created entry
		 */
		int32 NewEntry(const FConstPoint& Point);

		/**
		 * Stage an append operation for later merge. Thread-safe.
		 * @param EntryIndex Target entry index
		 * @param Point Point to append
		 */
		void Append(int32 EntryIndex, const FPoint& Point);

		/**
		 * Stage an append operation for later merge. Thread-safe.
		 * @param EntryIndex Target entry index
		 * @param Point Point to append
		 */
		void Append(int32 EntryIndex, const FConstPoint& Point);

		/**
		 * Merge all staged appends into target metadata.
		 * MUST be called from single thread after all concurrent operations complete.
		 */
		void Finalize();

		/** Reserve capacity in thread buffers */
		void Reserve(int32 ExpectedAppendsPerThread);
	};

#pragma endregion

#pragma region Union Metadata

	/**
	 * Container for multiple union data entries.
	 * Manages a collection of IUnionData instances, one per fused point/edge.
	 * 
	 * THREAD SAFETY:
	 * - Use BeginConcurrentBuild()/EndConcurrentBuild() for thread-safe construction
	 * - After EndConcurrentBuild(), all access is read-only and safe
	 * - _Unsafe methods are for single-threaded contexts only
	 */
	class PCGEXBLENDING_API FUnionMetadata : public TSharedFromThis<FUnionMetadata>
	{
		friend class FUnionMetadataBuilder;

	public:
		/** All union data entries */
		TArray<TSharedPtr<IUnionData>> Entries;

		/** If true, entries don't correspond to real source data (e.g., generated intersections) */
		bool bIsAbstract = false;

	private:
		/** Optional builder for concurrent construction - null when not building */
		TUniquePtr<FUnionMetadataBuilder> Builder;

	public:
		FUnionMetadata() = default;
		~FUnionMetadata() = default;

		/** Returns the number of entries */
		FORCEINLINE int32 Num() const { return Entries.Num(); }

		/**
		 * Pre-allocate entry array to specific size (for NewEntryAt pattern).
		 * NOT THREAD SAFE.
		 */
		void SetNum(int32 InNum);

		//=== Concurrent Build Mode ===//

		/**
		 * Begin concurrent build phase. Allocates builder and enables thread-safe operations.
		 * @param ReserveCount Expected number of entries (for pre-allocation)
		 */
		void BeginConcurrentBuild(int32 ReserveCount = 0);

		/**
		 * End concurrent build phase. Finalizes all staged operations and releases builder.
		 * MUST be called from single thread.
		 */
		void EndConcurrentBuild();

		/** Returns true if currently in concurrent build mode */
		FORCEINLINE bool IsBuildingConcurrently() const { return Builder.IsValid(); }

		/**
		 * Create a new entry. Thread-safe when Builder is active.
		 * @param Point Initial point for the entry
		 * @return Index of new entry
		 */
		FORCEINLINE int32 NewEntry(const FConstPoint& Point)
		{
			return Builder ? Builder->NewEntry(Point) : NewEntry_Unsafe(Point);
		}

		/**
		 * Append to existing entry. Thread-safe when Builder is active.
		 * @param Index Entry index
		 * @param Point Point to append
		 */
		FORCEINLINE void Append(const int32 Index, const FPoint& Point)
		{
			if (Builder) { Builder->Append(Index, Point); }
			else { Append_Unsafe(Index, Point); }
		}

		/**
		 * Append to existing entry. Thread-safe when Builder is active.
		 * @param Index Entry index
		 * @param Point Point to append
		 */
		FORCEINLINE void Append(const int32 Index, const FConstPoint& Point)
		{
			if (Builder) { Builder->Append(Index, Point); }
			else { Append_Unsafe(Index, Point); }
		}

		//=== Single-Threaded Mode (Legacy/Unsafe) ===//

		/**
		 * Create new entry. NOT THREAD SAFE.
		 * @param Point Initial point
		 * @return Index of new entry
		 */
		int32 NewEntry_Unsafe(const FConstPoint& Point);

		/**
		 * Create entry at specific index (requires prior SetNum call). NOT THREAD SAFE.
		 * @param ItemIndex Target index
		 * @return The created entry
		 */
		TSharedPtr<IUnionData> NewEntryAt_Unsafe(int32 ItemIndex);

		/**
		 * Append to existing entry. NOT THREAD SAFE.
		 */
		FORCEINLINE void Append_Unsafe(const int32 Index, const FPoint& Point)
		{
			Entries[Index]->Add(Point);
		}

		/**
		 * Append to existing entry. NOT THREAD SAFE.
		 */
		FORCEINLINE void Append_Unsafe(const int32 Index, const FConstPoint& Point)
		{
			Entries[Index]->Add(Point);
		}

		//=== Read-Only Access (Always Safe After Build) ===//

		/**
		 * Check if entry's IO sources overlap with given set.
		 * @param InIdx Entry index
		 * @param InIndices Set of IO indices to check against
		 * @return True if any overlap exists
		 */
		bool IOIndexOverlap(int32 InIdx, const TSet<int32>& InIndices) const;

		/**
		 * Get entry by index. Safe for concurrent read access.
		 * @param Index Entry index
		 * @return Entry or nullptr if invalid index
		 */
		FORCEINLINE TSharedPtr<IUnionData> Get(const int32 Index) const
		{
			return Entries.IsValidIndex(Index) ? Entries[Index] : nullptr;
		}
	};

#pragma endregion
}