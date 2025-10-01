// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include <functional>

#include "CoreMinimal.h"
#include "Details/PCGExMacros.h"

struct FPCGExContext;

namespace PCGExData
{
	class FPointIOCollection;
	class FPointIO;
	class FPointIOTaggedEntries;
	class FPointIOTaggedDictionary;
}

namespace PCGExClusterUtils
{
	using FProblem = TTuple<bool, FText>;

	enum class EProblem : uint8
	{
		None             = 0,
		DoubleMarking    = 1,
		VtxTagButNoMeta  = 2,
		EdgeTagButNoMeta = 3,
		NoTags           = 4,
		VtxDupes         = 5,
		RoamingEdges     = 6,
		RoamingVtx       = 7,
		VtxWrongPin      = 8,
		EdgeWrongPin     = 9,
	};

	static const TMap<EProblem, FProblem> EProblemLogs = {
		{EProblem::DoubleMarking, FProblem{false, FTEXT("Uh oh, a data is marked as both Vtx and Edges -- it will be ignored for safety.")}},
		{EProblem::VtxTagButNoMeta, FProblem{false, FTEXT("A Vtx-tagged input has no metadata and will be discarded.")}},
		{EProblem::EdgeTagButNoMeta, FProblem{false, FTEXT("An Edges-tagged input has no edge metadata and will be discarded.")}},
		{EProblem::NoTags, FProblem{false, FTEXT("An input data is neither tagged Vtx or Edges and will be ignored.")}},
		{EProblem::VtxDupes, FProblem{true, FTEXT("At least two Vtx inputs share the same PCGEx/Cluster tag. Only one will be processed.")}},
		{EProblem::RoamingEdges, FProblem{true, FTEXT("Some input edges have no associated vtx.")}},
		{EProblem::RoamingVtx, FProblem{true, FTEXT("Some input vtx have no associated edges.")}},
		{EProblem::VtxWrongPin, FProblem{true, FTEXT("There are vtx in the edge pin. They will be ignored.")}},
		{EProblem::EdgeWrongPin, FProblem{true, FTEXT("There are edges in the vtx pin. They will be ignored.")}}
	};

	class PCGEXTENDEDTOOLKIT_API FClusterDataLibrary : public TSharedFromThis<FClusterDataLibrary>
	{
		bool bDisableInvalidData = true;
		TArray<int32> ProblemsTracker;
		TSet<TSharedPtr<PCGExData::FPointIO>> Invalidated;

	public:
		TArray<TSharedPtr<PCGExData::FPointIO>> TaggedVtx;
		TArray<TSharedPtr<PCGExData::FPointIO>> TaggedEdges;
		using FPairedVtxFn = std::function<void()>;

		TSharedPtr<PCGExData::FPointIOTaggedDictionary> InputDictionary;

		explicit FClusterDataLibrary(const bool InDisableInvalidData);

		bool Build(const TSharedPtr<PCGExData::FPointIOCollection>& InMixedCollection);
		bool Build(const TSharedPtr<PCGExData::FPointIOCollection>& InVtxCollection, const TSharedPtr<PCGExData::FPointIOCollection>& InEdgeCollection);

		bool IsDataValid(const TSharedPtr<PCGExData::FPointIO>& InPointIO) const;

		TSharedPtr<PCGExData::FPointIOTaggedEntries> GetAssociatedEdges(const TSharedPtr<PCGExData::FPointIO>& InVtxIO) const;

		void PrintLogs(FPCGExContext* InContext, bool bSkipTrivial = false, bool bSkipImportant = false);

	protected:
		bool BuildDictionary();

		void Invalidate(const TSharedPtr<PCGExData::FPointIO>& InPointData, const EProblem Problem);
		void Log(const EProblem Problem);
	};
}
