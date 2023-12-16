// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Data/PCGExAttributeHelpers.h"
#include "PCGExEdge.h"

namespace PCGExMesh
{
	struct PCGEXTENDEDTOOLKIT_API FIndexedEdge : public PCGExGraph::FUnsignedEdge
	{
		int32 Index = -1;

		FIndexedEdge(const int32 InIndex, const int32 InStart, const int32 InEnd)
			: FUnsignedEdge(InStart, InEnd, EPCGExEdgeType::Complete),
			  Index(InIndex)
		{
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FVertex
	{
		int32 Index = -1;
		FVector Position;
		TArray<int32> Neighbors;
		TArray<int32> Edges;

		FVertex()
		{
			Index = -1;
			Position = FVector::ZeroVector;
			Neighbors.Empty();
			Edges.Empty();
		}

		~FVertex();

		void Add(const int32 EdgeIndex, const int32 VtxIndex);
	};

	struct PCGEXTENDEDTOOLKIT_API FMesh
	{
		int32 MeshID = -1;
		TMap<int32, int32> IndicesMap; //Mesh vertices
		TArray<FVertex> Vertices;
		TArray<FIndexedEdge> Edges;

		FMesh();

		~FMesh();

		void BuildFrom(const PCGExData::FPointIO& InPoints, const PCGExData::FPointIO& InEdges);
		int32 FindClosestVertex(const FVector& Position);

		FVertex& GetVertexFromPointIndex(int32 Index);
		FVertex& GetVertex(int32 Index);

	protected:
		FVertex& GetOrCreateVertex(int32 Index, bool& bJustCreated);
	};
}
