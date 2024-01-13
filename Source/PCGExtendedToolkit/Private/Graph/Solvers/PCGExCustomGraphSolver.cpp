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
	const FVector PtPosition = Point.Point->Transform.GetLocation();

	if (!Probe.LooseBounds.IsInside(PtPosition)) { return false; }

	const double PtDistance = FVector::DistSquared(Probe.Origin, PtPosition);
	if (PtDistance > Probe.MaxDistance) { return false; }
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
	OutProbes.Reset(SocketInfos.Num());
	double MaxDistance = 0.0;
	for (const PCGExGraph::FSocketInfos& CurrentSocketInfos : SocketInfos)
	{
		PCGExGraph::FSocketProbe& NewProbe = OutProbes.Emplace_GetRef(&CurrentSocketInfos);
		InitializeProbe(NewProbe);
		const double Dist = PrepareProbeForPointSocketPair(Point, NewProbe, CurrentSocketInfos);
		MaxDistance = FMath::Max(MaxDistance, Dist);
	}
	return MaxDistance;
}

double UPCGExCustomGraphSolver::PrepareProbeForPointSocketPair(
	const PCGEx::FPointRef& Point,
	PCGExGraph::FSocketProbe& Probe,
	const PCGExGraph::FSocketInfos& InSocketInfos) const
{
	const FPCGExSocketBounds& SocketBounds = InSocketInfos.Socket->Descriptor.Bounds;

	FVector Direction = SocketBounds.Direction;
	double DotTolerance = SocketBounds.DotThreshold;
	double MaxDistance = SocketBounds.MaxDistance;

	const FTransform PtTransform = Point.Point->Transform;
	FVector Origin = PtTransform.GetLocation();

	if (InSocketInfos.LocalDirectionGetter &&
		InSocketInfos.LocalDirectionGetter->bValid)
	{
		Direction = (*InSocketInfos.LocalDirectionGetter)[Point.Index];
	}

	if (InSocketInfos.Socket->Descriptor.bRelativeOrientation)
	{
		Direction = PtTransform.Rotator().RotateVector(Direction);
	}

	Direction.Normalize();

	if (InSocketInfos.MaxDistanceGetter &&
		InSocketInfos.MaxDistanceGetter->bValid)
	{
		MaxDistance *= (*InSocketInfos.MaxDistanceGetter)[Point.Index];
	}

	Probe.Direction = Direction;
	Probe.DotThreshold = DotTolerance;
	Probe.MaxDistance = MaxDistance * MaxDistance;
	Probe.DotOverDistanceCurve = SocketBounds.DotOverDistanceCurve;

	Direction.Normalize();
	FVector Offset = FVector::ZeroVector;
	switch (InSocketInfos.Socket->Descriptor.OffsetOrigin)
	{
	default:
	case EPCGExExtension::None:
		break;
	case EPCGExExtension::Extents:
		Offset = Direction * Point.Point->GetExtents();
		Origin += Offset;
		break;
	case EPCGExExtension::Scale:
		Offset = Direction * Point.Point->Transform.GetScale3D();
		Origin += Offset;
		break;
	case EPCGExExtension::ScaledExtents:
		Offset = Direction * Point.Point->GetScaledExtents();
		Origin += Offset;
		break;
	}

	Probe.Origin = Origin;
	MaxDistance += Offset.Length();

	if (DotTolerance >= 0) { Probe.LooseBounds = PCGExMath::ConeBox(Probe.Origin, Direction, MaxDistance); }
	else { Probe.LooseBounds = FBoxCenterAndExtent(Probe.Origin, FVector(MaxDistance)).GetBox(); }

	return MaxDistance;
}
