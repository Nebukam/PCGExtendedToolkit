// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Math/Geo/PCGExDelaunay.h"

#include "CoreMinimal.h"
#include "PCGExCoreSettingsCache.h"

#include "PCGExSettingsCacheBody.h"
#include "Math/Geo/PCGExPrimtives.h"
#include "ThirdParty/Delaunator/include/delaunator.hpp"
#include "Async/ParallelFor.h"
#include "Math/Geo/PCGExGeo.h"
#include "Math/PCGExProjectionDetails.h"

namespace PCGExMath::Geo
{
	FDelaunaySite2::FDelaunaySite2(const UE::Geometry::FIndex3i& InVtx, const UE::Geometry::FIndex3i& InAdjacency, const int32 InId)
		: Id(InId)
	{
		for (int i = 0; i < 3; i++)
		{
			Vtx[i] = InVtx[i];
			Neighbors[i] = InAdjacency[i];
		}
	}

	FDelaunaySite2::FDelaunaySite2(const int32 A, const int32 B, const int32 C, const int32 InId)
		: Id(InId)
	{
		Vtx[0] = A;
		Vtx[1] = B;
		Vtx[2] = C;
		for (int i = 0; i < 3; i++) { Neighbors[i] = -1; }
	}

	bool FDelaunaySite2::ContainsEdge(const uint64 Edge) const
	{
		return Edge == PCGEx::H64U(Vtx[0], Vtx[1]) || Edge == PCGEx::H64U(Vtx[0], Vtx[2]) || Edge == PCGEx::H64U(Vtx[1], Vtx[2]);
	}

	uint64 FDelaunaySite2::GetSharedEdge(const FDelaunaySite2* Other) const
	{
		return Other->ContainsEdge(PCGEx::H64U(Vtx[0], Vtx[1])) ? PCGEx::H64U(Vtx[0], Vtx[1]) : Other->ContainsEdge(PCGEx::H64U(Vtx[0], Vtx[2])) ? PCGEx::H64U(Vtx[0], Vtx[2]) : PCGEx::H64U(Vtx[1], Vtx[2]);
	}

	void FDelaunaySite2::PushAdjacency(const int32 SiteId)
	{
		for (int32& i : Neighbors)
		{
			if (i == -1)
			{
				i = SiteId;
				bOnHull = i != 2;
				break;
			}
		}
	}

	TDelaunay2::~TDelaunay2()
	{
		Clear();
	}

	void TDelaunay2::Clear()
	{
		Sites.Empty();
		DelaunayEdges.Empty();
		DelaunayHull.Empty();

		IsValid = false;
	}

	bool TDelaunay2::Process(const TArrayView<FVector>& Positions, const FPCGExGeo2DProjectionDetails& ProjectionDetails)
	{
		Clear();

		if (const int32 NumPositions = Positions.Num(); Positions.IsEmpty() || NumPositions <= 2) { return false; }

		TMap<uint64, int32> EdgeMap;

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Delaunator::Triangulate);

			IsValid = true;
			auto PushEdge = [&](FDelaunaySite2& Site, const uint64 Edge)
			{
				bool bIsAlreadySet = false;
				DelaunayEdges.Add(Edge, &bIsAlreadySet);
				if (bIsAlreadySet)
				{
					if (int32 Idx = -1; EdgeMap.RemoveAndCopyValue(Edge, Idx))
					{
						FDelaunaySite2& OtherSite = Sites[Idx];
						OtherSite.PushAdjacency(Site.Id);
						Site.PushAdjacency(OtherSite.Id);
					}
				}
				else
				{
					EdgeMap.Add(Edge, Site.Id);
				}
			};

			if (PCGEX_CORE_SETTINGS.bUseDelaunator)
			{
				std::vector<double> OutVector(Positions.Num() * 2);
				ProjectionDetails.Project(Positions, OutVector);

				delaunator::Delaunator d(OutVector);

				if (d.runtime_error) { return false; }

				const int32 NumTriangles = d.triangles.size();

				if (!NumTriangles) { return false; }

				const int32 NumSites = NumTriangles / 3;
				DelaunayEdges.Reserve(NumSites);
				Sites.Reserve(NumSites);
				EdgeMap.Reserve(NumSites);

				int32 s = 0;
				for (std::size_t i = 0; i < NumTriangles; i += 3)
				{
					FDelaunaySite2& Site = Sites.Emplace_GetRef(d.triangles[i], d.triangles[i + 1], d.triangles[i + 2], s++);
					PushEdge(Site, Site.AB());
					PushEdge(Site, Site.BC());
					PushEdge(Site, Site.AC());
				}
			}
			else
			{
				TArray<FVector2D> OutVector;
				ProjectionDetails.Project(Positions, OutVector);

				UE::Geometry::FDelaunay2 Delaunay2;
				if (!Delaunay2.Triangulate(OutVector)) { return false; }

				TArray<UE::Geometry::FIndex3i> Triangles = Delaunay2.GetTriangles();
				const int32 NumTriangles = Triangles.Num();

				if (!NumTriangles) { return false; }

				const int32 NumSites = NumTriangles / 3;
				DelaunayEdges.Reserve(NumSites);
				Sites.Reserve(NumSites);
				EdgeMap.Reserve(NumSites);

				int32 s = 0;
				for (const UE::Geometry::FIndex3i& T : Triangles)
				{
					FDelaunaySite2& Site = Sites.Emplace_GetRef(T.A, T.B, T.C, s++);
					PushEdge(Site, Site.AB());
					PushEdge(Site, Site.BC());
					PushEdge(Site, Site.AC());
				}
			}

			DelaunayEdges.Shrink();
			DelaunayHull.Reserve(DelaunayEdges.Num() / 3);
		}

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Delaunay2D::FillHullSites);

			for (const FDelaunaySite2& Site : Sites)
			{
				if (Site.bOnHull)
				{
					// Push edges that are still waiting to be matched
					if (EdgeMap.Contains(Site.AB()))
					{
						DelaunayHull.Add(Site.Vtx[0]);
						DelaunayHull.Add(Site.Vtx[1]);
					}

					if (EdgeMap.Contains(Site.BC()))
					{
						DelaunayHull.Add(Site.Vtx[1]);
						DelaunayHull.Add(Site.Vtx[2]);
					}

					if (EdgeMap.Contains(Site.AC()))
					{
						DelaunayHull.Add(Site.Vtx[0]);
						DelaunayHull.Add(Site.Vtx[2]);
					}
				}
			}
		}

		return IsValid;
	}

	void TDelaunay2::RemoveLongestEdges(const TArrayView<FVector>& Positions)
	{
		uint64 Edge;
		for (const FDelaunaySite2& Site : Sites)
		{
			GetLongestEdge(Positions, Site.Vtx, Edge);
			DelaunayEdges.Remove(Edge);
		}
	}

	void TDelaunay2::RemoveLongestEdges(const TArrayView<FVector>& Positions, TSet<uint64>& LongestEdges)
	{
		uint64 Edge;
		for (const FDelaunaySite2& Site : Sites)
		{
			GetLongestEdge(Positions, Site.Vtx, Edge);
			DelaunayEdges.Remove(Edge);
			LongestEdges.Add(Edge);
		}
	}

	void TDelaunay2::GetMergedSites(const int32 SiteIndex, const TSet<uint64>& EdgeConnectors, TSet<int32>& OutMerged, TSet<uint64>& OutUEdges, TBitArray<>& VisitedSites)

	{
		TArray<int32> Stack;

		VisitedSites[SiteIndex] = false;
		Stack.Add(SiteIndex);

		while (!Stack.IsEmpty())
		{
			const int32 NextIndex = Stack.Pop(EAllowShrinking::No);

			if (VisitedSites[NextIndex]) { continue; }

			OutMerged.Add(NextIndex);
			VisitedSites[NextIndex] = true;

			const FDelaunaySite2* Site = (Sites.GetData() + NextIndex);

			for (int i = 0; i < 3; i++)
			{
				const int32 OtherIndex = Site->Neighbors[i];
				if (OtherIndex == -1 || VisitedSites[OtherIndex]) { continue; }
				const FDelaunaySite2* NeighborSite = Sites.GetData() + OtherIndex;
				if (const uint64 SharedEdge = Site->GetSharedEdge(NeighborSite); EdgeConnectors.Contains(SharedEdge))
				{
					OutUEdges.Add(SharedEdge);
					Stack.Add(OtherIndex);
				}
			}
		}

		VisitedSites[SiteIndex] = true;
	}

	FDelaunaySite3::FDelaunaySite3(const FIntVector4& InVtx, const int32 InId)
		: Id(InId)
	{
		for (int i = 0; i < 4; i++)
		{
			Vtx[i] = InVtx[i];
			Faces[i] = 0;
		}

		Algo::Sort(Vtx);
	}

	void FDelaunaySite3::ComputeFaces()
	{
		for (int i = 0; i < 4; i++) { Faces[i] = PCGEx::UH3(Vtx[MTX[i][0]], Vtx[MTX[i][1]], Vtx[MTX[i][2]]); }
	}

	TDelaunay3::~TDelaunay3()
	{
		Clear();
	}

	void TDelaunay3::Clear()
	{
		Sites.Empty();
		DelaunayEdges.Empty();
		DelaunayHull.Empty();

		IsValid = false;
	}

	void TDelaunay3::RemoveLongestEdges(const TArrayView<FVector>& Positions)
	{
		uint64 Edge;
		for (const FDelaunaySite3& Site : Sites)
		{
			GetLongestEdge(Positions, Site.Vtx, Edge);
			DelaunayEdges.Remove(Edge);
		}
	}

	void TDelaunay3::RemoveLongestEdges(const TArrayView<FVector>& Positions, TSet<uint64>& LongestEdges)
	{
		uint64 Edge;
		for (const FDelaunaySite3& Site : Sites)
		{
			GetLongestEdge(Positions, Site.Vtx, Edge);
			DelaunayEdges.Remove(Edge);
			LongestEdges.Add(Edge);
		}
	}
}
