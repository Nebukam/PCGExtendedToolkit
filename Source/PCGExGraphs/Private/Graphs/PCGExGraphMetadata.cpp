// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graphs/PCGExGraphMetadata.h"

#include "PCGElement.h"
#include "Core/PCGExContext.h"
#include "Details/PCGExIntersectionDetails.h"
#include "Helpers/PCGExMetaHelpers.h"

namespace PCGExGraphs
{
	void FGraphMetadataDetails::Update(FPCGExContext* InContext, const FPCGExPointUnionMetadataDetails& InDetails)
	{
		bWriteIsPointUnion = InDetails.bWriteIsUnion;
		IsPointUnionAttributeName = InDetails.IsUnionAttributeName;
		PCGEX_SOFT_VALIDATE_NAME(bWriteIsPointUnion, IsPointUnionAttributeName, InContext)

		bWritePointUnionSize = InDetails.bWriteUnionSize;
		PointUnionSizeAttributeName = InDetails.UnionSizeAttributeName;
		PCGEX_SOFT_VALIDATE_NAME(bWritePointUnionSize, PointUnionSizeAttributeName, InContext)
	}

	void FGraphMetadataDetails::Update(FPCGExContext* InContext, const FPCGExEdgeUnionMetadataDetails& InDetails)
	{
		bWriteIsEdgeUnion = InDetails.bWriteIsUnion;
		IsEdgeUnionAttributeName = InDetails.IsUnionAttributeName;
		PCGEX_SOFT_VALIDATE_NAME(bWriteIsEdgeUnion, IsEdgeUnionAttributeName, InContext)

		bWriteIsSubEdge = InDetails.bWriteIsSubEdge;
		IsSubEdgeAttributeName = InDetails.IsSubEdgeAttributeName;
		PCGEX_SOFT_VALIDATE_NAME(bWriteIsSubEdge, IsSubEdgeAttributeName, InContext)

		bWriteEdgeUnionSize = InDetails.bWriteUnionSize;
		EdgeUnionSizeAttributeName = InDetails.UnionSizeAttributeName;
		PCGEX_SOFT_VALIDATE_NAME(bWriteEdgeUnionSize, EdgeUnionSizeAttributeName, InContext);
	}

	void FGraphMetadataDetails::Update(FPCGExContext* InContext, const FPCGExPointPointIntersectionDetails& InDetails)
	{
		Update(InContext, InDetails.PointUnionData);
		Update(InContext, InDetails.EdgeUnionData);
	}

	void FGraphMetadataDetails::Update(FPCGExContext* InContext, const FPCGExPointEdgeIntersectionDetails& InDetails)
	{
		bWriteIsIntersector = InDetails.bWriteIsIntersector;
		IsIntersectorAttributeName = InDetails.IsIntersectorAttributeName;
		PCGEX_SOFT_VALIDATE_NAME(bWriteIsIntersector, IsIntersectorAttributeName, InContext)
	}

	void FGraphMetadataDetails::Update(FPCGExContext* InContext, const FPCGExEdgeEdgeIntersectionDetails& InDetails)
	{
		bWriteCrossing = InDetails.bWriteCrossing;
		CrossingAttributeName = InDetails.CrossingAttributeName;
		PCGEX_SOFT_VALIDATE_NAME(bWriteCrossing, CrossingAttributeName, InContext);
	}

	FGraphNodeMetadata::FGraphNodeMetadata(const int32 InNodeIndex, const EPCGExIntersectionType InType)
		: NodeIndex(InNodeIndex), Type(InType)
	{
	}

	FGraphEdgeMetadata::FGraphEdgeMetadata(const int32 InEdgeIndex, const int32 InRootIndex, const EPCGExIntersectionType InType)
		: EdgeIndex(InEdgeIndex), RootIndex(InRootIndex < 0 ? InEdgeIndex : InRootIndex), Type(InType)
	{
	}

#undef PCGEX_FOREACH_EDGE_METADATA
}
