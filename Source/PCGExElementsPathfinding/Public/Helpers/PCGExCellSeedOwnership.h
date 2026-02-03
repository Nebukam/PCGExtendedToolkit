// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Clusters/Artifacts/PCGExCellDetails.h"
#include "Sorting/PCGExSortingCommon.h"
#include "Utils/PCGValueRange.h"

struct FPCGExContext;

namespace PCGExSorting
{
	class FSorter;
	class FSortCache;
}

namespace PCGExData
{
	class FFacade;
}

namespace PCGExCells
{
	/**
	 * Helper class for managing seed ownership selection.
	 * Encapsulates the logic for different ownership methods (SeedOrder, Closest, ClosestProjected, BestCandidate).
	 */
	class PCGEXELEMENTSPATHFINDING_API FSeedOwnershipHandler
	{
	public:
		FSeedOwnershipHandler() = default;

		EPCGExCellSeedOwnership Method = EPCGExCellSeedOwnership::SeedOrder;
		EPCGExSortDirection SortDirection = EPCGExSortDirection::Ascending;

		/** Initialize the ownership handler. Must be called before PickWinner. */
		bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InSeedsFacade);

		/** Whether we need to collect all candidates (vs break on first match). */
		FORCEINLINE bool NeedsAllCandidates() const { return Method != EPCGExCellSeedOwnership::SeedOrder; }

		/** Whether sorting rules pin is required. */
		FORCEINLINE bool RequiresSortingRules() const { return Method == EPCGExCellSeedOwnership::BestCandidate; }

		/** Whether initialized and ready to pick winners. */
		FORCEINLINE bool IsReady() const { return bInitialized; }

		/**
		 * Pick the winning seed index from a set of candidates.
		 * @param Candidates Array of seed indices competing for ownership
		 * @param CellCentroid Cell centroid position (used for Closest mode)
		 * @return The winning seed index, or INDEX_NONE if candidates is empty
		 */
		int32 PickWinner(const TArray<int32>& Candidates, const FVector& CellCentroid) const;

	private:
		bool bInitialized = false;
		TSharedPtr<PCGExSorting::FSorter> Sorter;
		TSharedPtr<PCGExSorting::FSortCache> SortCache;
		TConstPCGValueRange<FTransform> SeedTransforms; // For Closest mode
	};
}
