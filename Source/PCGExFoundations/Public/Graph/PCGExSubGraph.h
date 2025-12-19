// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include <functional>

#include "CoreMinimal.h"
#include "PCGExEdge.h"
#include "PCGExSortHelpers.h"
#include "Details/PCGExDetailsGraph.h"
#include "Utils/PCGValueRange.h"

class UPCGMetadata;
struct FPCGContext;

namespace PCGExMT
{
	struct FScope;
	class FTaskManager;
	class IAsyncHandleGroup;
}

struct FPCGExCarryOverDetails;
struct FPCGExEdgeUnionMetadataDetails;
struct FPCGExPointUnionMetadataDetails;
struct FPCGExEdgeEdgeIntersectionDetails;
struct FPCGExPointEdgeIntersectionDetails;
struct FPCGExPointPointIntersectionDetails;

namespace PCGExDetails
{
	class FDistances;
}

namespace PCGExData
{
	class FPointIOCollection;
	class FFacade;
	struct FConstPoint;
	struct FMutablePoint;
	class FUnionMetadata;
	template <typename T>
	class TBuffer;
}

namespace PCGExBlending
{
	class FUnionBlender;
}

struct FPCGExBlendingDetails;
struct FPCGExTransformDetails;

namespace PCGExGraph
{
	struct FLink;
	class FGraphBuilder;
	class FSubGraph;

}

namespace PCGExCluster
{
	class FCluster;
}


namespace PCGExGraph
{
	using NodeLinks = TArray<FLink, TInlineAllocator<8>>;

	PCGEX_CTX_STATE(State_PreparingUnion)
	PCGEX_CTX_STATE(State_ProcessingUnion)

	PCGEX_CTX_STATE(State_WritingClusters)
	PCGEX_CTX_STATE(State_ReadyToCompile)
	PCGEX_CTX_STATE(State_Compiling)

	PCGEX_CTX_STATE(State_ProcessingPointEdgeIntersections)
	PCGEX_CTX_STATE(State_ProcessingEdgeEdgeIntersections)

	PCGEX_CTX_STATE(State_Pathfinding)
	PCGEX_CTX_STATE(State_WaitingPathfinding)

	const TSet<FName> ProtectedClusterAttributes = {Attr_PCGExEdgeIdx, Attr_PCGExVtxIdx};

	class FGraph;
	
	class PCGEXTENDEDTOOLKIT_API FSubGraph : public TSharedFromThis<FSubGraph>
	{
	public:
		TWeakPtr<FGraph> WeakParentGraph;
		TArray<int32> Nodes;
		TArray<PCGEx::FIndexKey> Edges;
		TSet<int32> EdgesInIOIndices;
		TSharedPtr<PCGExData::FFacade> VtxDataFacade;
		TSharedPtr<PCGExData::FFacade> EdgesDataFacade;
		TArray<FEdge> FlattenedEdges;
		int32 UID = 0;
		FSubGraphPostProcessCallback OnSubGraphPostProcess;


		FSubGraph() = default;

		~FSubGraph() = default;

		void Add(const FEdge& Edge);
		void Shrink();

		void BuildCluster(const TSharedRef<PCGExCluster::FCluster>& InCluster);
		int32 GetFirstInIOIndex();

		void Compile(const TWeakPtr<PCGExMT::IAsyncHandleGroup>& InParentHandle, const TSharedPtr<PCGExMT::FTaskManager>& TaskManager, const TSharedPtr<FGraphBuilder>& InBuilder);

	protected:
		TWeakPtr<PCGExMT::FTaskManager> WeakTaskManager;
		TWeakPtr<FGraphBuilder> WeakBuilder;

		const FGraphMetadataDetails* MetadataDetails = nullptr;

		TSharedPtr<PCGExBlending::FUnionBlender> UnionBlender;

		// Edge metadata
#define PCGEX_FOREACH_EDGE_METADATA(MACRO)\
MACRO(IsEdgeUnion, bool, false, IsUnion()) \
MACRO(IsSubEdge, bool, false, bIsSubEdge) \
MACRO(EdgeUnionSize, int32, 0, UnionSize)

#define PCGEX_EDGE_METADATA_DECL(_NAME, _TYPE, _DEFAULT, _ACCESSOR) TSharedPtr<PCGExData::TBuffer<_TYPE>> _NAME##Buffer;
		PCGEX_FOREACH_EDGE_METADATA(PCGEX_EDGE_METADATA_DECL)
#undef PCGEX_EDGE_METADATA_DECL

#undef PCGEX_FOREACH_EDGE_METADATA

		// Extra edge data
		TSharedPtr<PCGExData::TBuffer<double>> EdgeLength;

		void CompileRange(const PCGExMT::FScope& Scope);
		void CompilationComplete();
	};


}
