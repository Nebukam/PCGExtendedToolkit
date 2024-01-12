// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExCluster.h"

#include "Data/PCGExAttributeHelpers.h"

namespace PCGExCluster
{
	FVertex::~FVertex()
	{
		Neighbors.Empty();
		Edges.Empty();
	}

	void FVertex::AddNeighbor(const int32 EdgeIndex, const int32 VertexIndex)
	{
		Edges.AddUnique(EdgeIndex);
		Neighbors.AddUnique(VertexIndex);
	}

	bool FVertex::GetNormal(FCluster* InCluster, FVector& OutNormal)
	{
		if (Neighbors.IsEmpty()) { return false; }

		for (int32 I : Neighbors)
		{
			FVector E1 = (InCluster->Vertices[I].Position - Position).GetSafeNormal();
			FVector Perp = FVector::CrossProduct(FVector::UpVector, E1).GetSafeNormal();
			OutNormal += FVector::CrossProduct(E1, Perp).GetSafeNormal();
		}

		OutNormal /= static_cast<double>(Neighbors.Num());

		return true;
	}

	FCluster::FCluster()
	{
		IndicesMap.Empty();
		Vertices.Empty();
		Edges.Empty();
		Bounds = FBox(ForceInit);
	}

	FCluster::~FCluster()
	{
		Vertices.Empty();
		IndicesMap.Empty();
		Edges.Empty();
	}

	FVertex& FCluster::GetOrCreateVertex(const int32 PointIndex, bool& bJustCreated)
	{
		if (const int32* VertexIndex = IndicesMap.Find(PointIndex))
		{
			bJustCreated = false;
			return Vertices[*VertexIndex];
		}

		bJustCreated = true;
		FVertex& Vertex = Vertices.Emplace_GetRef();
		const int32 VtxIndex = Vertices.Num() - 1;
		IndicesMap.Add(PointIndex, VtxIndex);

		Vertex.PointIndex = PointIndex;
		Vertex.ClusterIndex = VtxIndex;

		return Vertex;
	}

	void FCluster::BuildFrom(const PCGExData::FPointIO& InPoints, const PCGExData::FPointIO& InEdges)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCluster::BuildCluster);

		bHasInvalidEdges = false;

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

			if (!InVerticesPoints.IsValidIndex(VtxStart) ||
				!InVerticesPoints.IsValidIndex(VtxEnd) ||
				VtxStart == VtxEnd)
			{
				bHasInvalidEdges = true;
				continue;
			}

			Edges.Emplace_GetRef(i, VtxStart, VtxEnd);
			bool JustCreated = false;

			FVertex& Start = GetOrCreateVertex(VtxStart, JustCreated);
			if (JustCreated)
			{
				Start.Position = InVerticesPoints[VtxStart].Transform.GetLocation();
				Bounds += Start.Position;
			}

			FVertex& End = GetOrCreateVertex(VtxEnd, JustCreated);
			if (JustCreated)
			{
				End.Position = InVerticesPoints[VtxEnd].Transform.GetLocation();
				Bounds += End.Position;
			}

			Start.AddNeighbor(i, End.ClusterIndex);
			End.AddNeighbor(i, Start.ClusterIndex);
		}

		PCGEX_DELETE(StartIndexReader)
		PCGEX_DELETE(EndIndexReader)
	}

	int32 FCluster::FindClosestVertex(const FVector& Position) const
	{
		double MaxDistance = TNumericLimits<double>::Max();
		int32 ClosestIndex = -1;
		for (const FVertex& Vtx : Vertices)
		{
			const double Dist = FVector::DistSquared(Position, Vtx.Position);
			if (Dist < MaxDistance)
			{
				MaxDistance = Dist;
				ClosestIndex = Vtx.ClusterIndex;
			}
		}

		return ClosestIndex;
	}

	const FVertex& FCluster::GetVertexFromPointIndex(const int32 Index) const { return GetVertex(*IndicesMap.Find(Index)); }
	const FVertex& FCluster::GetVertex(const int32 Index) const { return Vertices[Index]; }
}
