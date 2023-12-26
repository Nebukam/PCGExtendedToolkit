// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExMesh.h"

#include "Data/PCGExAttributeHelpers.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Kismet/KismetMathLibrary.h"

#define PCGEX_FOREACH_TETRA_EDGE(_ACCESSOR,_EDGE_A, _EDGE_B, _BODY)\
for (int i = 0; i < 4; i++){ const FDelaunayVertex* _EDGE_A = _ACCESSOR[i];	for (int j = i + 1; j < 4; j++)	{ const FDelaunayVertex* _EDGE_B = _ACCESSOR[j]; _BODY }}

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

	FTetrahedron::FTetrahedron(FDelaunayVertex* InVtx1, FDelaunayVertex* InVtx2, FDelaunayVertex* InVtx3, FDelaunayVertex* InVtx4)
	{
		Vtx[0] = InVtx1;
		Vtx[1] = InVtx2;
		Vtx[2] = InVtx3;
		Vtx[3] = InVtx4;

		std::sort(std::begin(Vtx), std::end(Vtx), [](FDelaunayVertex* A, FDelaunayVertex* B) { return A->PointIndex < B->PointIndex; });

		bValid = PCGExMath::FindSphereFrom4Points(
			Vtx[0]->Position,
			Vtx[1]->Position,
			Vtx[2]->Position,
			Vtx[3]->Position,
			Circumsphere);
	}

	bool FTetrahedron::IsInside(const FVector& Point) const
	{
		const FVector U = Vtx[1]->Position - Vtx[0]->Position;
		const FVector V = Vtx[2]->Position - Vtx[0]->Position;
		const FVector W = Vtx[3]->Position - Vtx[0]->Position;
		const FVector P = Point - Vtx[0]->Position;

		const float DetTetrahedron = U | (V ^ W);
		const float Alpha = (P | (V ^ W)) / DetTetrahedron;
		const float Beta = (U | (P ^ W)) / DetTetrahedron;
		const float Gamma = (U | (V ^ P)) / DetTetrahedron;
		const float Delta = 1.0f - Alpha - Beta - Gamma;

		return (Alpha >= 0.0f && Beta >= 0.0f && Gamma >= 0.0f && Delta >= 0.0f && Delta <= 1.0f);
	}

	FDelaunayVertex* FTetrahedron::GetOppositeVertex(FDelaunayVertex* A, FDelaunayVertex* B) const
	{
		// Assuming that the tetrahedron vertices are ordered in a consistent way
		if (A != Vtx[0] && A != Vtx[1] && A != Vtx[2] && A != Vtx[3])
		{
			return A;
		}
		else
		{
			return B;
		}
	}

	bool FTetrahedron::Contains(const FDelaunayVertex* Vertex) const
	{
		for (const FDelaunayVertex* InVtx : Vtx) { if (InVtx == Vertex) { return true; } }
		return false;
	}

	bool FTetrahedron::ContainsEdge(const FDelaunayVertex* A, const FDelaunayVertex* B) const
	{
		PCGEX_FOREACH_TETRA_EDGE(Vtx, CVtx, OVtx, if ((A == CVtx && B == OVtx) || (B == CVtx && A == OVtx)) { return true; })
		return false;
	}

	bool FTetrahedron::HasSharedEdge(const FTetrahedron* OtherTetrahedron) const
	{
		PCGEX_FOREACH_TETRA_EDGE(Vtx, A, B, if (OtherTetrahedron->ContainsEdge(A, B)) { return true; })
		return false;
	}

	bool FTetrahedron::GetSharedEdge(const FTetrahedron* OtherTetrahedron, FDelaunayVertex& A, FDelaunayVertex& B) const
	{
		PCGEX_FOREACH_TETRA_EDGE(
			Vtx, CVtx, OVtx,
			if (OtherTetrahedron->ContainsEdge(CVtx, OVtx))
			{
			A = *CVtx;
			B = *OVtx;
			return true;
			})
		return false;
	}

	bool FTetrahedron::FlipEdge(FTetrahedron* OtherTetrahedron)
	{
		// Find the common edge
		FDelaunayVertex& A = *Vtx[0];
		FDelaunayVertex& B = *Vtx[0];
		if (!GetSharedEdge(OtherTetrahedron, A, B)) { return false; }

		// Flip the edge by updating vertices
		FDelaunayVertex* oppositeVertex1 = GetOppositeVertex(&A, &B);
		FDelaunayVertex* oppositeVertex2 = OtherTetrahedron->GetOppositeVertex(&A, &B);

		// Update vertices to flip the edge
		Vtx[0] = &A;
		Vtx[1] = &B;
		Vtx[2] = oppositeVertex2;

		OtherTetrahedron->Vtx[0] = &A;
		OtherTetrahedron->Vtx[1] = &B;
		OtherTetrahedron->Vtx[3] = oppositeVertex1;

		return true; // Indicate that the edge was flipped
	}

	bool FTetrahedron::SharedFace(const FTetrahedron* OtherTetrahedron, FDelaunayVertex& A, FDelaunayVertex& B, FDelaunayVertex& C) const
	{
		if ((Vtx[0] == OtherTetrahedron->Vtx[0] || Vtx[0] == OtherTetrahedron->Vtx[1] || Vtx[0] == OtherTetrahedron->Vtx[2]) &&
			(Vtx[1] == OtherTetrahedron->Vtx[0] || Vtx[1] == OtherTetrahedron->Vtx[1] || Vtx[1] == OtherTetrahedron->Vtx[2]) &&
			(Vtx[2] == OtherTetrahedron->Vtx[0] || Vtx[2] == OtherTetrahedron->Vtx[1] || Vtx[2] == OtherTetrahedron->Vtx[2]))
		{
			// Assign A B C
		}
		return false;
	}

	void FTetrahedron::RegisterEdges(TSet<uint64>& UniqueEdges, TArray<PCGExGraph::FUnsignedEdge>& Edges) const
	{
		PCGEX_FOREACH_TETRA_EDGE(
			Vtx, CVtx, OVtx, if (OVtx->PointIndex != -1)
			{
			PCGExGraph::FUnsignedEdge Edge = PCGExGraph::FUnsignedEdge(CVtx->PointIndex, OVtx->PointIndex, EPCGExEdgeType::Complete);
			if (uint64 EdgeHash = Edge.GetUnsignedHash();
				!UniqueEdges.Contains(EdgeHash))
			{
			UniqueEdges.Add(EdgeHash);
			Edges.Add(Edge);
			}
			})
	}

	void FTetrahedron::Draw(const UWorld* World) const
	{
		PCGEX_FOREACH_TETRA_EDGE(Vtx, A, B, DrawDebugLine(World, A->Position, B->Position, FColor::Red, true, 0, 0, 1);)
		DrawDebugSphere(World, Circumsphere.Center, Circumsphere.W, 32, FColor::Green, true, -1, 0, 1);
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

		for (const TPair<uint64, FTetrahedron*>& Pair : Tetrahedrons) { delete Pair.Value; }

		Tetrahedrons.Empty();
	}

	bool FDelaunayTriangulation::PrepareFrom(const PCGExData::FPointIO& PointIO)
	{
		if (PointIO.GetNum() <= 4) { return false; }

		FVector Min = FVector(TNumericLimits<double>::Max());
		FVector Max = FVector(TNumericLimits<double>::Min());
		FVector Centroid = FVector::Zero();

		const TArray<FPCGPoint>& Points = PointIO.GetIn()->GetPoints();
		int32 NumPoints = Points.Num();
		Vertices.SetNum(NumPoints + 4);
		for (int i = 0; i < NumPoints; i++)
		{
			FDelaunayVertex& Vtx = Vertices[i];
			Vtx.MeshIndex = i;
			Vtx.PointIndex = i;
			Vtx.Position = Points[i].Transform.GetLocation();
			Min = PCGExDataBlending::Min(Min, Vtx.Position);
			Max = PCGExDataBlending::Max(Max, Vtx.Position);
			Centroid += Vtx.Position;
		}

		Centroid /= static_cast<double>(NumPoints);

		double Radius = TNumericLimits<double>::Min();
		for (FDelaunayVertex& Vtx : Vertices)
		{
			Vtx.Dist = FVector::DistSquared(Vtx.Position, Centroid);
			Radius = FMath::Max(Radius, Vtx.Dist);
		}

		Vertices.Sort([&](const FDelaunayVertex& A, const FDelaunayVertex& B) { return A.Dist < B.Dist; });

		Radius = FMath::Sqrt(Radius);

		for (int i = NumPoints; i < NumPoints + 4; i++)
		{
			FDelaunayVertex& Vtx = Vertices[i];
			Vtx.MeshIndex = i;
			Vtx.PointIndex = -1;
		}

		// Super tetrahedron
		Vertices[NumPoints].Position = FVector(Centroid.X, Centroid.Y + Radius * 4, Centroid.Z - Radius * 3);
		Vertices[NumPoints + 1].Position = FVector(Centroid.X - Radius * 4, Centroid.Y - Radius * 4, Centroid.Z - Radius * 3);
		Vertices[NumPoints + 2].Position = FVector(Centroid.X + Radius * 4, Centroid.Y - Radius * 4, Centroid.Z - Radius * 3);
		Vertices[NumPoints + 3].Position = FVector(Centroid.X, Centroid.Y, Centroid.Z + Radius * 3);

		return EmplaceTetrahedron(&Vertices[NumPoints], &Vertices[NumPoints + 1], &Vertices[NumPoints + 2], &Vertices[NumPoints + 3])->bValid;
	}

	FTetrahedron* FDelaunayTriangulation::EmplaceTetrahedron(FDelaunayVertex* InVtx1, FDelaunayVertex* InVtx2, FDelaunayVertex* InVtx3, FDelaunayVertex* InVtx4)
	{
		const uint64 NTUID = TUID++;
		FTetrahedron* NewTetrahedron = new FTetrahedron(InVtx1, InVtx2, InVtx3, InVtx4);
		Tetrahedrons.Add(NTUID, NewTetrahedron);
		return NewTetrahedron;
	}

	void FDelaunayTriangulation::InsertVertex(const int32 InIndex)
	{
		const int32 Index = ++CurrentIndex;
		FWriteScopeLock WriteLock(TetraLock);

		TQueue<uint64> DeprecatedTetrahedrons;
		FDelaunayVertex* Vtx = &Vertices[Index];

		int32 insideCount = 0;
		UE_LOG(LogTemp, Warning, TEXT("Inserting %d (T Num = %d)"), Index, Tetrahedrons.Num());
		for (const TPair<uint64, FTetrahedron*>& Pair : Tetrahedrons)
		{
			if (Pair.Value->Circumsphere.IsInside(Vtx->Position) && Pair.Value->IsInside(Vtx->Position))
			{
				DeprecatedTetrahedrons.Enqueue(Pair.Key);
				insideCount++;
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("Inserting %d (T Inside = %d)"), Index, insideCount);
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

	bool FDelaunayTriangulation::IsUnsharedEdge(const FTetrahedron* Tetrahedron, const FDelaunayVertex* A, const FDelaunayVertex* B)
	{
		for (const TPair<uint64, FTetrahedron*>& Pair : Tetrahedrons)
		{
			if (Pair.Value != Tetrahedron) { if (Pair.Value->ContainsEdge(A, B)) { return true; } }
		}
		return false;
	}

	void FDelaunayTriangulation::FindNeighbors(const FTetrahedron* Tetrahedron, TArray<FTetrahedron*>& OutNeighbors)
	{
		for (const TPair<uint64, FTetrahedron*>& Pair : Tetrahedrons)
		{
			if (Pair.Value != Tetrahedron) { if (Tetrahedron->HasSharedEdge(Pair.Value)) { OutNeighbors.Add(Pair.Value); } }
		}
	}

	void FDelaunayTriangulation::FindNeighborsWithVertex(const FTetrahedron* Tetrahedron, const FDelaunayVertex* Vertex, TArray<FTetrahedron*>& OutNeighbors)
	{
		for (const TPair<uint64, FTetrahedron*>& Pair : Tetrahedrons)
		{
			if (Pair.Value != Tetrahedron) { if (Tetrahedron->Contains(Vertex)) { OutNeighbors.Add(Pair.Value); } }
		}
	}

	void FDelaunayTriangulation::FindEdges()
	{
		// Make it Delaunay compliant
		bool modificationFlag = true;

		while (modificationFlag)
		{
			modificationFlag = false;

			for (const TPair<uint64, FTetrahedron*>& Pair : Tetrahedrons)
			{
				// Assume GetEdges is a method that returns pairs of edges for the tetrahedron
				for (int i = 0; i < 4; i++)
				{
					for (int j = i + 1; j < 4; j++)
					{
						check(i!=j)
						FDelaunayVertex* A = Pair.Value->Vtx[i];
						FDelaunayVertex* B = Pair.Value->Vtx[j];
						
						if (A != B && IsUnsharedEdge(Pair.Value, A, B))
						{
							TArray<FTetrahedron*> Neighbors;
							FindNeighbors(Pair.Value, Neighbors);

							// Calculate the opposite vertex
							FDelaunayVertex* oppositeVertex = Pair.Value->GetOppositeVertex(A, B);

							// Check if the opposite vertex is inside the circumcircle of the neighboring tetrahedron
							FindNeighborsWithVertex(Pair.Value, oppositeVertex, Neighbors);
							for (FTetrahedron* Neighbor : Neighbors)
							{
								if (Neighbor->Circumsphere.IsInside(oppositeVertex->Position))
								{
									// Flip the edge
									if (Pair.Value->FlipEdge(Neighbor))
									{
										// Set the modification flag to true if an edge was flipped
										modificationFlag = true;
									}
								}
							}
						}
					}
				}
			}
		}


		// Remove supertetrahedron
		if (FTetrahedron** Tetrahedron = Tetrahedrons.Find(0))
		{
			Tetrahedrons.Remove(0);
			delete *Tetrahedron;
		}

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

#undef PCGEX_FOREACH_TETRA_EDGE
