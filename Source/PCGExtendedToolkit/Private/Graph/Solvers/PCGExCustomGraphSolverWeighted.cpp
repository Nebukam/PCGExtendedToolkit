// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Solvers/PCGExCustomGraphSolverWeighted.h"

void UPCGExCustomGraphSolverWeighted::InitializeProbe(PCGExGraph::FSocketProbe& Probe) const
{
	Probe.Candidates.Empty();
}

bool UPCGExCustomGraphSolverWeighted::ProcessPoint(
	PCGExGraph::FSocketProbe& Probe,
	const PCGEx::FPointRef& Point) const
{
	const FVector PtPosition = Probe.GetTargetCenter(*Point.Point);

	if (!Probe.CompoundBounds.IsInside(PtPosition)) { return false; }

	const double PtDistance = FVector::DistSquared(Probe.Origin, PtPosition);
	if (PtDistance > Probe.Radius) { return false; }

	const double Dot = Probe.Direction.Dot((PtPosition - Probe.Origin).GetSafeNormal());
	if (Dot < Probe.DotThreshold) { return false; }

	Probe.ProbedDistanceMin = FMath::Min(Probe.ProbedDistanceMin, PtDistance);
	Probe.ProbedDistanceMax = FMath::Max(Probe.ProbedDistanceMax, PtDistance);
	Probe.ProbedDotMin = FMath::Min(Probe.ProbedDotMin, Dot);
	Probe.ProbedDotMax = FMath::Max(Probe.ProbedDotMax, Dot);

	PCGExGraph::FPointCandidate& Candidate = Probe.Candidates.Emplace_GetRef();

	Candidate.Dot = Dot;
	Candidate.Distance = PtDistance;
	Candidate.Index = Point.Index;

	return true;
}

void UPCGExCustomGraphSolverWeighted::ResolveProbe(PCGExGraph::FSocketProbe& Probe) const
{
	PCGExGraph::FPointCandidate& BestCandidate = Probe.BestCandidate;

	for (const PCGExGraph::FPointCandidate& Candidate : Probe.Candidates)
	{
		const double DotRating = 1 - PCGExMath::Remap(Candidate.Dot, Probe.ProbedDotMin, Probe.ProbedDotMax);
		const double DistanceRating = PCGExMath::Remap(Candidate.Distance, Probe.ProbedDistanceMin, Probe.ProbedDistanceMax);
		const double DotWeight = FMath::Clamp(Probe.DotOverDistanceCurve->GetFloatValue(DistanceRating), 0, 1);
		const double Rating = (DotRating * DotWeight) + (DistanceRating * (1 - DotWeight));

		bool bBetterCandidate = false;
		if (Rating < Probe.IndexedRating || BestCandidate.Index == -1)
		{
			bBetterCandidate = true;
		}
		else if (Rating == Probe.IndexedRating)
		{
			if (DotWeight > Probe.IndexedDotWeight)
			{
				if (DotRating < Probe.IndexedDotRating ||
					(DotRating == Probe.IndexedDotRating && DistanceRating < Probe.IndexedDistanceRating))
				{
					bBetterCandidate = true;
				}
			}
			else
			{
				if (DistanceRating < Probe.IndexedDistanceRating ||
					(DistanceRating == Probe.IndexedRating && DotRating < Probe.IndexedDotRating))
				{
					bBetterCandidate = true;
				}
			}
		}

		if (bBetterCandidate)
		{
			Probe.IndexedRating = Rating;
			Probe.IndexedDistanceRating = DistanceRating;
			Probe.IndexedDotRating = DotRating;
			Probe.IndexedDotWeight = DotWeight;

			BestCandidate.Index = Candidate.Index;
		}
	}
}
