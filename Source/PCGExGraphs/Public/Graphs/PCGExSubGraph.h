// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGraphCommon.h"
#include "Clusters/PCGExEdge.h"
#include "PCGExH.h"

namespace PCGExBlending
{
	class FUnionBlender;
}

namespace PCGExMT
{
	struct FScope;
	class FTaskManager;
	class IAsyncHandleGroup;
}

namespace PCGExClusters
{
	class FCluster;
}

namespace PCGExData
{
	class FFacade;

	template <typename T>
	class TBuffer;
}

namespace PCGExGraphs
{
	struct FGraphMetadataDetails;
	class FGraph;

	class PCGEXGRAPHS_API FSubGraph : public TSharedFromThis<FSubGraph>
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

		void BuildCluster(const TSharedRef<PCGExClusters::FCluster>& InCluster);
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
