// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include <functional>

#include "CoreMinimal.h"
#include "PCGExGraphCommon.h"
#include "Data/PCGExDataCommon.h"
#include "Graphs/PCGExGraphDetails.h"
#include "Utils/PCGValueRange.h"

namespace PCGEx
{
	class FIndexLookup;
}

namespace PCGExData
{
	class FPointIOCollection;
	class FFacade;
}

namespace PCGExMT
{
	class FTaskManager;
}

namespace PCGExGraphs
{
	struct FGraphMetadataDetails;
	class FGraph;

	class PCGEXGRAPHS_API FGraphBuilder : public TSharedFromThis<FGraphBuilder>
	{
	protected:
		TSharedPtr<PCGExMT::FTaskManager> TaskManager;
		const FGraphMetadataDetails* MetadataDetailsPtr = nullptr;
		bool bWriteVtxDataFacadeWithCompile = false;
		bool bCompiling = false;

	public:
		const FPCGExGraphBuilderDetails* OutputDetails = nullptr;

		FGraphCompilationEndCallback OnCompilationEndCallback;
		FSubGraphPostProcessCallback OnSubGraphPostProcess;

		PCGExDataId PairId;
		TSharedPtr<FGraph> Graph;

		TSharedRef<PCGExData::FFacade> NodeDataFacade;
		TSharedPtr<PCGEx::FIndexLookup> NodeIndexLookup;

		// The collection of edges given to the node
		// We need the full collection even if unrelated, because we track data by index
		// and those indices are relative to the input data, not the graph context
		TSharedPtr<PCGExData::FPointIOCollection> EdgesIO;
		const TArray<TSharedRef<PCGExData::FFacade>>* SourceEdgeFacades = nullptr;

		// Used exclusively by the custom graph builder.
		// Otherwise a transient array is allocated for the duration of the graph compilation
		TSharedPtr<TArray<int32>> OutputNodeIndices;
		TSharedPtr<TArray<int32>> OutputPointIndices;

		// A value range we will fetch positions from during compilation
		// It must have a valid range for Node.PointIndex
		TConstPCGValueRange<FTransform> NodePointsTransforms;

		// This is true by default, but should be disabled for edge cases where we create new points from scratch
		// especially if the final amount of points is greater than the number of points we're trying to inherit from.
		bool bInheritNodeData = true;

		// This will be set to true post-graph compilation, if compilation was a success
		bool bCompiledSuccessfully = false;

		FGraphBuilder(const TSharedRef<PCGExData::FFacade>& InNodeDataFacade, const FPCGExGraphBuilderDetails* InDetails);

		const FGraphMetadataDetails* GetMetadataDetails() const { return MetadataDetailsPtr; }

		void CompileAsync(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager, const bool bWriteNodeFacade, const FGraphMetadataDetails* MetadataDetails = nullptr);
		void Compile(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager, const bool bWriteNodeFacade, const FGraphMetadataDetails* MetadataDetails = nullptr);

	protected:
		void OnCompilationEnd();

	public:
		void StageEdgesOutputs() const;
		void MoveEdgesOutputs(const TSharedPtr<PCGExData::FPointIOCollection>& To, const int32 IndexOffset) const;

		~FGraphBuilder() = default;
	};
}
