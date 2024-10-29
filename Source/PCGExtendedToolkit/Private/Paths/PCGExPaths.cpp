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

void PCGExPaths::FPathEdgeLength::ProcessEdge(const FPath* Path, const FPathEdge& Edge)
{
	const double Dist = FVector::Dist(Path->GetPosUnsafe(Edge.Start), Path->GetPosUnsafe(Edge.End));
	GetMutable(Edge.Start) = Dist;
	TotalLength += Dist;
}

void PCGExPaths::FPathEdgeLengthSquared::ProcessEdge(const FPath* Path, const FPathEdge& Edge)
{
	const double Dist = FVector::DistSquared(Path->GetPosUnsafe(Edge.Start), Path->GetPosUnsafe(Edge.End));
	GetMutable(Edge.Start) = Dist;
}

void PCGExPaths::FPathEdgeNormal::ProcessEdge(const FPath* Path, const FPathEdge& Edge)
{
	GetMutable(Edge.Start) = FVector::CrossProduct(Up, Path->DirToNextPoint(Edge.Start)).GetSafeNormal();
}

void PCGExPaths::FPathEdgeBinormal::ProcessFirstEdge(const FPath* Path, const FPathEdge& Edge)
{
	const FVector B = Path->DirToNextPoint(Edge.Start);
	const FVector C = FVector::CrossProduct(Up, B).GetSafeNormal();
	
	Normals[Edge.Start] = C;
	GetMutable(Edge.Start) = C;
}

void PCGExPaths::FPathEdgeBinormal::ProcessEdge(const FPath* Path, const FPathEdge& Edge)
{
	const FVector A = Path->DirToPrevPoint(Edge.Start);
	const FVector B = Path->DirToNextPoint(Edge.Start);
	const FVector C = FVector::CrossProduct(Up, B).GetSafeNormal();
	
	Normals[Edge.Start] = C;

	FVector D = FQuat(FVector::CrossProduct(A, B).GetSafeNormal(), FMath::Acos(FVector::DotProduct(A, B)) * 0.5f).RotateVector(A);

	if (FVector::DotProduct(C, D) < 0.0f) { D *= -1; }

	GetMutable(Edge.Start) = D;
}

void PCGExPaths::FPathEdgeBinormal::ProcessLastEdge(const FPath* Path, const FPathEdge& Edge)
{
	const FVector B = Path->DirToNextPoint(Edge.Start);
	const FVector C = FVector::CrossProduct(Up, B).GetSafeNormal();
	
	Normals[Edge.Start] = C;
	GetMutable(Edge.Start) = C;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
