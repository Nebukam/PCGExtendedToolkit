// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExMesh.h"

#include "Data/PCGExAttributeHelpers.h"
#include "Data/Blending/PCGExDataBlending.h"

namespace PCGExMesh
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

	FTetrahedron::FTetrahedron(FVertex* InVtx1, FVertex* InVtx2, FVertex* InVtx3, FVertex* InVtx4)
	{
		Vtx[0] = InVtx1;
		Vtx[1] = InVtx2;
		Vtx[2] = InVtx3;
		Vtx[3] = InVtx4;

		std::sort(std::begin(Vtx), std::end(Vtx), [](FVertex* A, FVertex* B) { return A->PointIndex < B->PointIndex; });

		Det = Determinant3X3(
				Vtx[0]->Position.X - Vtx[3]->Position.X, Vtx[0]->Position.Y - Vtx[3]->Position.Y, Vtx[0]->Position.Z - Vtx[3]->Position.Z,
				Vtx[1]->Position.X - Vtx[3]->Position.X, Vtx[1]->Position.Y - Vtx[3]->Position.Y, Vtx[1]->Position.Z - Vtx[3]->Position.Z,
				Vtx[2]->Position.X - Vtx[3]->Position.X, Vtx[2]->Position.Y - Vtx[3]->Position.Y, Vtx[2]->Position.Z - Vtx[3]->Position.Z
			);

		Det = Det * Det;
	}

	bool FTetrahedron::IsInCircumsphere(const FVector& Position)
	{
		const double Det1 = Determinant3X3(
				Position.X - Vtx[0]->Position.X, Position.Y - Vtx[0]->Position.Y, Position.Z - Vtx[0]->Position.Z,
				Vtx[1]->Position.X - Vtx[0]->Position.X, Vtx[1]->Position.Y - Vtx[0]->Position.Y, Vtx[1]->Position.Z - Vtx[0]->Position.Z,
				Vtx[2]->Position.X - Vtx[0]->Position.X, Vtx[2]->Position.Y - Vtx[0]->Position.Y, Vtx[2]->Position.Z - Vtx[0]->Position.Z
			);

		const double Det2 = Determinant3X3(
				Vtx[1]->Position.X - Vtx[0]->Position.X, Vtx[1]->Position.Y - Vtx[0]->Position.Y, Vtx[1]->Position.Z - Vtx[0]->Position.Z,
				Position.X - Vtx[0]->Position.X, Position.Y - Vtx[0]->Position.Y, Position.Z - Vtx[0]->Position.Z,
				Vtx[2]->Position.X - Vtx[0]->Position.X, Vtx[2]->Position.Y - Vtx[0]->Position.Y, Vtx[2]->Position.Z - Vtx[0]->Position.Z
			);

		const double Det3 = Determinant3X3(
				Vtx[2]->Position.X - Vtx[0]->Position.X, Vtx[2]->Position.Y - Vtx[0]->Position.Y, Vtx[2]->Position.Z - Vtx[0]->Position.Z,
				Vtx[1]->Position.X - Vtx[0]->Position.X, Vtx[1]->Position.Y - Vtx[0]->Position.Y, Vtx[1]->Position.Z - Vtx[0]->Position.Z,
				Position.X - Vtx[0]->Position.X, Position.Y - Vtx[0]->Position.Y, Position.Z - Vtx[0]->Position.Z
			);

		return (Det1 * Det1 + Det2 * Det2 + Det3 * Det3 - Det) > 0;
	}

	bool FTetrahedron::SharedFace(const FTetrahedron* OtherTetrahedron, FVertex& A, FVertex& B, FVertex& C) const
	{
		if ((Vtx[0] == OtherTetrahedron->Vtx[0] || Vtx[0] == OtherTetrahedron->Vtx[1] || Vtx[0] == OtherTetrahedron->Vtx[2]) &&
			(Vtx[1] == OtherTetrahedron->Vtx[0] || Vtx[1] == OtherTetrahedron->Vtx[1] || Vtx[1] == OtherTetrahedron->Vtx[2]) &&
			(Vtx[2] == OtherTetrahedron->Vtx[0] || Vtx[2] == OtherTetrahedron->Vtx[1] || Vtx[2] == OtherTetrahedron->Vtx[2]))
		{
			// Assign A B C
		}
		return false;
	}

	void FTetrahedron::RegisterEdges(TSet<uint64>& UniqueEdges, TArray<PCGExGraph::FUnsignedEdge>& Edges)
	{
		PCGExGraph::FUnsignedEdge Edge = PCGExGraph::FUnsignedEdge(Vtx[0]->PointIndex, Vtx[1]->PointIndex, EPCGExEdgeType::Complete);
		uint64 EdgeHash = Edge.GetUnsignedHash();
		if (!UniqueEdges.Contains(EdgeHash))
		{
			UniqueEdges.Add(EdgeHash);
			Edges.Add(Edge);
		}

		Edge = PCGExGraph::FUnsignedEdge(Vtx[1]->PointIndex, Vtx[2]->PointIndex, EPCGExEdgeType::Complete);
		EdgeHash = Edge.GetUnsignedHash();
		if (!UniqueEdges.Contains(EdgeHash))
		{
			UniqueEdges.Add(EdgeHash);
			Edges.Add(Edge);
		}

		Edge = PCGExGraph::FUnsignedEdge(Vtx[2]->PointIndex, Vtx[3]->PointIndex, EPCGExEdgeType::Complete);
		EdgeHash = Edge.GetUnsignedHash();
		if (!UniqueEdges.Contains(EdgeHash))
		{
			UniqueEdges.Add(EdgeHash);
			Edges.Add(Edge);
		}

		Edge = PCGExGraph::FUnsignedEdge(Vtx[0]->PointIndex, Vtx[3]->PointIndex, EPCGExEdgeType::Complete);
		EdgeHash = Edge.GetUnsignedHash();
		if (!UniqueEdges.Contains(EdgeHash))
		{
			UniqueEdges.Add(EdgeHash);
			Edges.Add(Edge);
		}
	}

	void FTetrahedron::Draw(const UWorld* World) const
	{
		for (int i = 0; i < 4; i++)
		{
			const FVertex* A = Vtx[i];
			for (int j = 0; j < 4; j++)
			{
				if (i != j)
				{
					const FVertex* B = Vtx[j];
					DrawDebugLine(World, A->Position, B->Position, FColor::Red, true, 0, 0, 1);
				}
			}
		}
	}

	FDelaunayTriangulation::FDelaunayTriangulation()
	{
		Vertices.Empty();
		Edges.Empty();
		Tetrahedrons.Empty();
	}

	FDelaunayTriangulation::~FDelaunayTriangulation()
	{
		Vertices.Empty();
		Edges.Empty();
		Tetrahedrons.Empty();
	}

	void FDelaunayTriangulation::PrepareFrom(const PCGExData::FPointIO& PointIO)
	{
		FVector Min = FVector(TNumericLimits<double>::Max());
		FVector Max = FVector(TNumericLimits<double>::Min());
		FVector Centroid = FVector::Zero();

		const TArray<FPCGPoint>& Points = PointIO.GetIn()->GetPoints();
		int32 NumPoints = Points.Num();
		Vertices.SetNum(NumPoints + 4);
		for (int i = 0; i < NumPoints; i++)
		{
			FVertex& Vtx = Vertices[i];
			Vtx.MeshIndex = i;
			Vtx.PointIndex = i;
			Vtx.Position = Points[i].Transform.GetLocation();
			Min = PCGExDataBlending::Min(Min, Vtx.Position);
			Max = PCGExDataBlending::Max(Max, Vtx.Position);
			Centroid += Vtx.Position;
		}

		Centroid /= static_cast<double>(NumPoints);

		double Radius = TNumericLimits<double>::Min();
		for (const FVertex& Vtx : Vertices)
		{
			Radius = FMath::Max(Radius, FVector::DistSquared(Vtx.Position, Centroid));
		}

		Radius = FMath::Sqrt(Radius);

		for (int i = NumPoints; i < NumPoints + 4; i++)
		{
			FVertex& Vtx = Vertices[i];
			Vtx.MeshIndex = i;
			Vtx.PointIndex = i;
		}

		// Super tetrahedron
		Vertices[NumPoints].Position = FVector(Centroid.X, Centroid.Y + Radius * 2, Centroid.Z - Radius);
		Vertices[NumPoints + 1].Position = FVector(Centroid.X - Radius, Centroid.Y - Radius, Centroid.Z - Radius);
		Vertices[NumPoints + 2].Position = FVector(Centroid.X + Radius, Centroid.Y - Radius, Centroid.Z - Radius);
		Vertices[NumPoints + 3].Position = FVector(Centroid.X, Centroid.Y, Centroid.Z + Radius * 2);

		EmplaceTetrahedron(&Vertices[NumPoints], &Vertices[NumPoints + 1], &Vertices[NumPoints + 2], &Vertices[NumPoints + 3]);
	}

	FTetrahedron* FDelaunayTriangulation::EmplaceTetrahedron(FVertex* InVtx1, FVertex* InVtx2, FVertex* InVtx3, FVertex* InVtx4)
	{
		const uint64 NTUID = TUID++;
		FTetrahedron* NewTetrahedron = new FTetrahedron(InVtx1, InVtx2, InVtx3, InVtx4);
		Tetrahedrons.Add(NTUID, NewTetrahedron);
		return NewTetrahedron;
	}

	void FDelaunayTriangulation::InsertVertex(const int32 Index)
	{
		FWriteScopeLock WriteLock(TetraLock);

		TQueue<uint64> DeprecatedTetrahedrons;
		FVertex* Vtx = &Vertices[Index];

		for (const TPair<uint64, FTetrahedron*>& Pair : Tetrahedrons)
		{
			if (Pair.Value->IsInCircumsphere(Vtx->Position))
			{
				DeprecatedTetrahedrons.Enqueue(Pair.Key);
			}
		}

		uint64 TKey;

		while (DeprecatedTetrahedrons.Dequeue(TKey))
		{
			const FTetrahedron* Tetrahedron = *Tetrahedrons.Find(TKey);
			Tetrahedrons.Remove(TKey);

			EmplaceTetrahedron(Vtx, Tetrahedron->Vtx[0], Tetrahedron->Vtx[1], Tetrahedron->Vtx[2]);
			EmplaceTetrahedron(Vtx, Tetrahedron->Vtx[0], Tetrahedron->Vtx[1], Tetrahedron->Vtx[3]);
			EmplaceTetrahedron(Vtx, Tetrahedron->Vtx[0], Tetrahedron->Vtx[2], Tetrahedron->Vtx[3]);
			EmplaceTetrahedron(Vtx, Tetrahedron->Vtx[1], Tetrahedron->Vtx[2], Tetrahedron->Vtx[3]);

			delete Tetrahedron;
		}
	}

	void FDelaunayTriangulation::FindEdges()
	{
		// Remove supertetrahedron
		const FTetrahedron* Tetrahedron = *Tetrahedrons.Find(0);
		Tetrahedrons.Remove(0);
		delete Tetrahedron;

		TSet<uint64> UniqueEdges;
		UniqueEdges.Reserve(Tetrahedrons.Num());

		for (const TPair<uint64, FTetrahedron*>& Pair : Tetrahedrons)
		{
			Pair.Value->RegisterEdges(UniqueEdges, Edges);
		}

		UniqueEdges.Empty();
	}

	FMesh::FMesh()
	{
		IndicesMap.Empty();
		Vertices.Empty();
		Edges.Empty();
		Bounds = FBox(ForceInit);
	}

	FMesh::~FMesh()
	{
		Vertices.Empty();
		IndicesMap.Empty();
		Edges.Empty();
	}

	FVertex& FMesh::GetOrCreateVertex(const int32 PointIndex, bool& bJustCreated)
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
		Vertex.MeshIndex = VtxIndex;

		return Vertex;
	}

	void FMesh::BuildFrom(const PCGExData::FPointIO& InPoints, const PCGExData::FPointIO& InEdges)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExMesh::BuildMesh);

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
				!InVerticesPoints.IsValidIndex(VtxEnd))
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

			Start.AddNeighbor(i, End.MeshIndex);
			End.AddNeighbor(i, Start.MeshIndex);
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
				ClosestIndex = Vtx.MeshIndex;
			}
		}

		return ClosestIndex;
	}

	const FVertex& FMesh::GetVertexFromPointIndex(const int32 Index) const { return GetVertex(*IndicesMap.Find(Index)); }
	const FVertex& FMesh::GetVertex(const int32 Index) const { return Vertices[Index]; }
}
