// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Solvers/PCGExCustomGraphSolver.h"

void UPCGExCustomGraphSolver::InitializeProbe(PCGExGraph::FSocketProbe& Probe) const
{
}

bool UPCGExCustomGraphSolver::ProcessPoint(
	PCGExGraph::FSocketProbe& Probe,
	const PCGEx::FPointRef& Point) const
{
	const FVector PtPosition = Probe.GetTargetCenter(*Point.Point);

	if (!Probe.CompoundBounds.IsInside(PtPosition)) { return false; }

	const double PtDistance = FVector::DistSquared(Probe.Origin, PtPosition);
	if (PtDistance > Probe.Radius) { return false; }
	if (PtDistance > Probe.BestCandidate.Distance) { return false; }

	const double Dot = Probe.Direction.Dot((PtPosition - Probe.Origin).GetSafeNormal());
	if (Dot < Probe.DotThreshold) { return false; }

	Probe.BestCandidate.Dot = Dot;
	Probe.BestCandidate.Distance = PtDistance;
	Probe.BestCandidate.Index = Point.Index;

	return true;
}

void UPCGExCustomGraphSolver::ResolveProbe(PCGExGraph::FSocketProbe& Probe) const
{
}

double UPCGExCustomGraphSolver::PrepareProbesForPoint(
	const TArray<PCGExGraph::FSocketInfos>& SocketInfos,
	const PCGEx::FPointRef& Point,
	TArray<PCGExGraph::FSocketProbe>& OutProbes) const
{
	const int32 NumSockets = SocketInfos.Num();
	OutProbes.SetNum(NumSockets);
	double MaxRadius = 0.0;
	for (int i = 0; i < NumSockets; i++)
	{
		const PCGExGraph::FSocketInfos& CurrentSocketInfos = SocketInfos[i];
		PCGExGraph::FSocketProbe& NewProbe = OutProbes[i];

		NewProbe.SocketInfos = &CurrentSocketInfos;
		InitializeProbe(NewProbe);

		const double Dist = PrepareProbeForPointSocketPair(Point, NewProbe, CurrentSocketInfos);
		MaxRadius = FMath::Max(MaxRadius, Dist);
	}
	return FMath::Sqrt(MaxRadius);
}

double UPCGExCustomGraphSolver::PrepareProbeForPointSocketPair(
	const PCGEx::FPointRef& Point,
	PCGExGraph::FSocketProbe& Probe,
	const PCGExGraph::FSocketInfos& InSocketInfos) const
{
	InSocketInfos.Socket->GetDirection(Point.Index, Probe.Direction);
	InSocketInfos.Socket->GetDotThreshold(Point.Index, Probe.DotThreshold);
	InSocketInfos.Socket->GetRadius(Point.Index, Probe.Radius);

	const FTransform PtTransform = Point.Point->Transform;
	const FVector ProbeOrigin = PtTransform.GetLocation();

	if (InSocketInfos.Socket->Descriptor.bRelativeOrientation)
	{
		Probe.Direction = PtTransform.TransformVector(Probe.Direction);
	}

	Probe.Direction.Normalize();
	Probe.DotOverDistanceCurve = InSocketInfos.Socket->Descriptor.DotOverDistanceCurve;

	Probe.Origin = InSocketInfos.Socket->Descriptor.DistanceSettings.GetSourceCenter(
		*Point.Point, ProbeOrigin, ProbeOrigin + Probe.Direction * Probe.Radius);
	Probe.Radius += (Probe.Origin - ProbeOrigin).Length();

	if (Probe.DotThreshold >= 0) { Probe.CompoundBounds = PCGExMath::ConeBox(Probe.Origin, Probe.Direction, Probe.Radius); }
	else { Probe.CompoundBounds = FBoxCenterAndExtent(Probe.Origin, FVector(Probe.Radius)).GetBox(); }

	Probe.Radius = Probe.Radius * Probe.Radius;
	return Probe.Radius;
}
