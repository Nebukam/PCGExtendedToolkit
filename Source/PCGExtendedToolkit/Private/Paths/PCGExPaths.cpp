// Copyright 2025 Timothé Lapetite and contributors
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
		for (const TSharedPtr<FPathEdgeExtraBase>& Extra : Extras) { Extra->ProcessSingleEdge(this, Edges[0]); }
	}
	else
	{
		if (Index == 0) { for (const TSharedPtr<FPathEdgeExtraBase>& Extra : Extras) { Extra->ProcessFirstEdge(this, Edges[0]); } }
		else if (Index == LastEdge) { for (const TSharedPtr<FPathEdgeExtraBase>& Extra : Extras) { Extra->ProcessLastEdge(this, Edges[LastEdge]); } }
		else { for (const TSharedPtr<FPathEdgeExtraBase>& Extra : Extras) { Extra->ProcessEdge(this, Edges[Index]); } }
	}
}

void PCGExPaths::FPath::ExtraComputingDone()
{
	for (const TSharedPtr<FPathEdgeExtraBase>& Extra : Extras) { Extra->ProcessingDone(this); }
	Extras.Empty(); // So we don't update them anymore
}

void PCGExPaths::FPath::ComputeAllEdgeExtra()
{
	if (NumEdges == 1)
	{
		for (const TSharedPtr<FPathEdgeExtraBase>& Extra : Extras) { Extra->ProcessSingleEdge(this, Edges[0]); }
	}
	else
	{
		for (const TSharedPtr<FPathEdgeExtraBase>& Extra : Extras) { Extra->ProcessFirstEdge(this, Edges[0]); }
		for (int i = 1; i < LastEdge; i++) { for (const TSharedPtr<FPathEdgeExtraBase>& Extra : Extras) { Extra->ProcessEdge(this, Edges[i]); } }
		for (const TSharedPtr<FPathEdgeExtraBase>& Extra : Extras) { Extra->ProcessLastEdge(this, Edges[LastEdge]); }
	}

	ExtraComputingDone();
}

namespace PCGExPaths
{
#pragma region FPathEdgeLength

	void FPathEdgeLength::ProcessEdge(const FPath* Path, const FPathEdge& Edge)
	{
		const double Dist = FVector::Dist(Path->GetPos_Unsafe(Edge.Start), Path->GetPos_Unsafe(Edge.End));
		GetMutable(Edge.Start) = Dist;
		TotalLength += Dist;
	}

	void FPathEdgeLength::ProcessingDone(const FPath* Path)
	{
		TPathEdgeExtra<double>::ProcessingDone(Path);
		CumulativeLength.SetNumUninitialized(Data.Num());
		CumulativeLength[0] = Data[0];
		for (int i = 1; i < Data.Num(); i++) { CumulativeLength[i] = CumulativeLength[i - 1] + Data[i]; }
	}

#pragma endregion

#pragma region FPathEdgeLength

	void FPathEdgeLengthSquared::ProcessEdge(const FPath* Path, const FPathEdge& Edge)
	{
		const double Dist = FVector::DistSquared(Path->GetPos_Unsafe(Edge.Start), Path->GetPos_Unsafe(Edge.End));
		GetMutable(Edge.Start) = Dist;
	}

#pragma endregion

#pragma region FPathEdgeNormal

	void FPathEdgeNormal::ProcessEdge(const FPath* Path, const FPathEdge& Edge)
	{
		GetMutable(Edge.Start) = FVector::CrossProduct(Up, Edge.Dir).GetSafeNormal();
	}

#pragma endregion

#pragma region FPathEdgeBinormal

	void FPathEdgeBinormal::ProcessFirstEdge(const FPath* Path, const FPathEdge& Edge)
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

	void FPathEdgeBinormal::ProcessEdge(const FPath* Path, const FPathEdge& Edge)
	{
		const FVector N = FVector::CrossProduct(Up, Edge.Dir).GetSafeNormal();
		Normals[Edge.Start] = N;

		const FVector A = Path->DirToPrevPoint(Edge.Start);
		FVector D = FQuat(FVector::CrossProduct(A, Edge.Dir).GetSafeNormal(), FMath::Acos(FVector::DotProduct(A, Edge.Dir)) * 0.5f).RotateVector(A);

		if (FVector::DotProduct(N, D) < 0.0f) { D *= -1; }

		GetMutable(Edge.Start) = D;
	}

	void FPathEdgeBinormal::ProcessLastEdge(const FPath* Path, const FPathEdge& Edge)
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

	void FPathEdgeAvgNormal::ProcessFirstEdge(const FPath* Path, const FPathEdge& Edge)
	{
		if (Path->IsClosedLoop())
		{
			ProcessEdge(Path, Edge);
			return;
		}

		GetMutable(Edge.Start) = FVector::CrossProduct(Up, Edge.Dir).GetSafeNormal();
	}

	void FPathEdgeAvgNormal::ProcessEdge(const FPath* Path, const FPathEdge& Edge)
	{
		const FVector A = FVector::CrossProduct(Up, Path->DirToPrevPoint(Edge.Start) * -1).GetSafeNormal();
		const FVector B = FVector::CrossProduct(Up, Edge.Dir).GetSafeNormal();
		GetMutable(Edge.Start) = FMath::Lerp(A, B, 0.5).GetSafeNormal();
	}

	void FPathEdgeAvgNormal::ProcessLastEdge(const FPath* Path, const FPathEdge& Edge)
	{
		if (Path->IsClosedLoop())
		{
			ProcessEdge(Path, Edge);
			return;
		}

		GetMutable(Edge.Start) = FVector::CrossProduct(Up, Edge.Dir).GetSafeNormal();
	}

#pragma endregion

#pragma region FPathEdgeHalfAngle

	void FPathEdgeHalfAngle::ProcessFirstEdge(const FPath* Path, const FPathEdge& Edge)
	{
		if (Path->IsClosedLoop())
		{
			ProcessEdge(Path, Edge);
			return;
		}

		GetMutable(Edge.Start) = PI;
	}

	void FPathEdgeHalfAngle::ProcessEdge(const FPath* Path, const FPathEdge& Edge)
	{
		GetMutable(Edge.Start) = FMath::Acos(FVector::DotProduct(Path->DirToPrevPoint(Edge.Start), Edge.Dir));
	}

	void FPathEdgeHalfAngle::ProcessLastEdge(const FPath* Path, const FPathEdge& Edge)
	{
		if (Path->IsClosedLoop())
		{
			ProcessEdge(Path, Edge);
			return;
		}

		GetMutable(Edge.Start) = PI;
	}

#pragma endregion

#pragma region FPathEdgeAngle

	void FPathEdgeFullAngle::ProcessFirstEdge(const FPath* Path, const FPathEdge& Edge)
	{
		if (Path->IsClosedLoop())
		{
			ProcessEdge(Path, Edge);
			return;
		}

		GetMutable(Edge.Start) = PI;
	}

	void FPathEdgeFullAngle::ProcessEdge(const FPath* Path, const FPathEdge& Edge)
	{
		GetMutable(Edge.Start) = PCGExMath::GetAngle(Path->DirToPrevPoint(Edge.Start) * -1, Edge.Dir);
	}

	void FPathEdgeFullAngle::ProcessLastEdge(const FPath* Path, const FPathEdge& Edge)
	{
		if (Path->IsClosedLoop())
		{
			ProcessEdge(Path, Edge);
			return;
		}

		GetMutable(Edge.Start) = PI;
	}

#pragma endregion
}
#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
