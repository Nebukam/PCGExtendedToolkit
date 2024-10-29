// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPaths.h"

#define LOCTEXT_NAMESPACE "PCGExPaths"
#define PCGEX_NAMESPACE PCGExPaths

void FPCGExPathFilterSettings::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
}

bool FPCGExPathFilterSettings::Init(FPCGExContext* InContext)
{
	return true;
}

void PCGExPaths::FPath::ComputeEdgeExtra(const int32 Index)
{
	if (NumEdges == 1)
	{
		for (const TSharedPtr<FPathEdgeExtraBase> Extra : Extras) { Extra->ProcessSingleEdge(this, Edges[0]); }
	}
	else
	{
		if (Index == 0) { for (const TSharedPtr<FPathEdgeExtraBase> Extra : Extras) { Extra->ProcessFirstEdge(this, Edges[0]); } }
		else if (Index == LastEdge) { for (const TSharedPtr<FPathEdgeExtraBase> Extra : Extras) { Extra->ProcessLastEdge(this, Edges[LastEdge]); } }
		else { for (const TSharedPtr<FPathEdgeExtraBase> Extra : Extras) { Extra->ProcessEdge(this, Edges[Index]); } }
	}
}

void PCGExPaths::FPath::ComputeAllEdgeExtra()
{
	if (NumEdges == 1)
	{
		for (const TSharedPtr<FPathEdgeExtraBase> Extra : Extras) { Extra->ProcessSingleEdge(this, Edges[0]); }
	}
	else
	{
		for (const TSharedPtr<FPathEdgeExtraBase> Extra : Extras) { Extra->ProcessFirstEdge(this, Edges[0]); }
		for (int i = 1; i < LastEdge; i++) { for (const TSharedPtr<FPathEdgeExtraBase> Extra : Extras) { Extra->ProcessEdge(this, Edges[i]); } }
		for (const TSharedPtr<FPathEdgeExtraBase> Extra : Extras) { Extra->ProcessLastEdge(this, Edges[LastEdge]); }
	}

	Extras.Empty(); // So we don't update them anymore
}

#pragma region FPathEdgeLength

void PCGExPaths::FPathEdgeLength::ProcessEdge(const FPath* Path, const FPathEdge& Edge)
{
	const double Dist = FVector::Dist(Path->GetPosUnsafe(Edge.Start), Path->GetPosUnsafe(Edge.End));
	GetMutable(Edge.Start) = Dist;
	TotalLength += Dist;
}

#pragma endregion

#pragma region FPathEdgeLength

void PCGExPaths::FPathEdgeLengthSquared::ProcessEdge(const FPath* Path, const FPathEdge& Edge)
{
	const double Dist = FVector::DistSquared(Path->GetPosUnsafe(Edge.Start), Path->GetPosUnsafe(Edge.End));
	GetMutable(Edge.Start) = Dist;
}

#pragma endregion

#pragma region FPathEdgeNormal

void PCGExPaths::FPathEdgeNormal::ProcessEdge(const FPath* Path, const FPathEdge& Edge)
{
	GetMutable(Edge.Start) = FVector::CrossProduct(Up, Edge.Dir).GetSafeNormal();
}

#pragma endregion

#pragma region FPathEdgeBinormal

void PCGExPaths::FPathEdgeBinormal::ProcessFirstEdge(const FPath* Path, const FPathEdge& Edge)
{
	if (Path->IsClosedLoop())
	{
		ProcessEdge(Path, Edge);
		return;
	}

	const FVector N = FVector::CrossProduct(Up, Edge.Dir).GetSafeNormal();
	Normals[Edge.Start] = N;
	GetMutable(Edge.Start) = N;
}

void PCGExPaths::FPathEdgeBinormal::ProcessEdge(const FPath* Path, const FPathEdge& Edge)
{
	const FVector N = FVector::CrossProduct(Up, Edge.Dir).GetSafeNormal();
	Normals[Edge.Start] = N;

	const FVector A = Path->DirToPrevPoint(Edge.Start);
	FVector D = FQuat(FVector::CrossProduct(A, Edge.Dir).GetSafeNormal(), FMath::Acos(FVector::DotProduct(A, Edge.Dir)) * 0.5f).RotateVector(A);

	if (FVector::DotProduct(N, D) < 0.0f) { D *= -1; }

	GetMutable(Edge.Start) = D;
}

void PCGExPaths::FPathEdgeBinormal::ProcessLastEdge(const FPath* Path, const FPathEdge& Edge)
{
	if (Path->IsClosedLoop())
	{
		ProcessEdge(Path, Edge);
		return;
	}

	const FVector C = FVector::CrossProduct(Up, Edge.Dir).GetSafeNormal();

	Normals[Edge.Start] = C;
	GetMutable(Edge.Start) = C;
}

#pragma endregion

#pragma region FPathEdgeAvgNormal

void PCGExPaths::FPathEdgeAvgNormal::ProcessFirstEdge(const FPath* Path, const FPathEdge& Edge)
{
	if (Path->IsClosedLoop())
	{
		ProcessEdge(Path, Edge);
		return;
	}

	GetMutable(Edge.Start) = FVector::CrossProduct(Up, Edge.Dir).GetSafeNormal();
}

void PCGExPaths::FPathEdgeAvgNormal::ProcessEdge(const FPath* Path, const FPathEdge& Edge)
{
	const FVector A = FVector::CrossProduct(Up, Path->DirToPrevPoint(Edge.Start) * -1).GetSafeNormal();
	const FVector B = FVector::CrossProduct(Up, Edge.Dir).GetSafeNormal();
	GetMutable(Edge.Start) = FMath::Lerp(A, B, 0.5).GetSafeNormal();
}

void PCGExPaths::FPathEdgeAvgNormal::ProcessLastEdge(const FPath* Path, const FPathEdge& Edge)
{
	if (Path->IsClosedLoop())
	{
		ProcessEdge(Path, Edge);
		return;
	}

	GetMutable(Edge.Start) = FVector::CrossProduct(Up, Edge.Dir).GetSafeNormal();
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
