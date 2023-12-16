// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExMesh.h"

namespace PCGExMesh
{
	FVertex::~FVertex()
	{
		Neighbors.Empty();
		Edges.Empty();
	}

	void FVertex::Add(const int32 EdgeIndex, const int32 VtxIndex)
	{
		Edges.AddUnique(EdgeIndex);
		Neighbors.AddUnique(VtxIndex);
	}

	FMesh::FMesh()
	{
		IndicesMap.Empty();
		Vertices.Empty();
		Edges.Empty();
	}

	FMesh::~FMesh()
	{
		Vertices.Empty();
		IndicesMap.Empty();
		Edges.Empty();
	}

	FVertex& FMesh::GetOrCreateVertex(const int32 Index, bool& bJustCreated)
	{
		if (const int32* VertexIndex = IndicesMap.Find(Index))
		{
			bJustCreated = false;
			return Vertices[*VertexIndex];
		}

		bJustCreated = true;
		FVertex& Vertex = Vertices.Emplace_GetRef();
		IndicesMap.Add(Index, Vertices.Num() - 1);

		Vertex.Index = Index;

		return Vertex;
	}

	void FMesh::BuildFrom(const PCGExData::FPointIO& InPoints, const PCGExData::FPointIO& InEdges)
	{
		const TArray<FPCGPoint>& InVerticesPoints = InPoints.GetIn()->GetPoints();
		const int32 NumVertices = InVerticesPoints.Num();
		Vertices.Reset(NumVertices);
		IndicesMap.Empty(NumVertices);

		const TArray<FPCGPoint>& InEdgesPoints = InEdges.GetIn()->GetPoints();
		const int32 NumEdges = InEdgesPoints.Num();
		Edges.Reset(NumEdges);

		PCGEx::TFAttributeReader<int32>* StartIndexReader = new PCGEx::TFAttributeReader<int32>(PCGExGraph::EdgeStartAttributeName);
		PCGEx::TFAttributeReader<int32>* EndIndexReader = new PCGEx::TFAttributeReader<int32>(PCGExGraph::EdgeEndAttributeName);

		StartIndexReader->Bind(const_cast<PCGExData::FPointIO&>(InEdges));
		EndIndexReader->Bind(const_cast<PCGExData::FPointIO&>(InEdges));

		for (int i = 0; i < NumEdges; i++)
		{
			int32 VtxStart = StartIndexReader->Values[i];
			int32 VtxEnd = EndIndexReader->Values[i];

			FIndexedEdge& Edge = Edges.Emplace_GetRef(i, VtxStart, VtxEnd);
			bool JustCreated = false;

			FVertex& Start = GetOrCreateVertex(VtxStart, JustCreated);
			if (JustCreated) { Start.Position = InVerticesPoints[VtxStart].Transform.GetLocation(); }
			Start.Add(i, VtxEnd);

			FVertex& End = GetOrCreateVertex(VtxEnd, JustCreated);
			if (JustCreated) { Start.Position = InVerticesPoints[VtxEnd].Transform.GetLocation(); }
			End.Add(i, VtxStart);
		}

		PCGEX_DELETE(StartIndexReader)
		PCGEX_DELETE(EndIndexReader)
	}

	int32 FMesh::FindClosestVertex(const FVector& Position) const
	{
		double MaxDistance = TNumericLimits<double>::Max();
		int32 ClosestIndex = -1;
		for (const FVertex& Vtx : Vertices)
		{
			const double Dist = FVector::DistSquared(Position, Vtx.Position);
			if (Dist < MaxDistance)
			{
				MaxDistance = Dist;
				ClosestIndex = Vtx.Index;
			}
		}

		return ClosestIndex;
	}

	const FVertex& FMesh::GetVertexFromPointIndex(const int32 Index) const { return GetVertex(*IndicesMap.Find(Index)); }
	const FVertex& FMesh::GetVertex(const int32 Index) const { return Vertices[Index]; }
}
