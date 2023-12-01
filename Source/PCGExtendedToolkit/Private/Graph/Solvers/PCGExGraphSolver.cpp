// Fill out your copyright notice in the Description page of Project Settings.


#include "Graph/Solvers/PCGExGraphSolver.h"

void UPCGExGraphSolver::InitializeProbe(PCGExGraph::FSocketProbe& Probe) const
{
}

bool UPCGExGraphSolver::ProcessPoint(
	PCGExGraph::FSocketProbe& Probe,
	const FPCGPoint* Point,
	const int32 Index) const
{
	const FVector PtPosition = Point->Transform.GetLocation();

	if (!Probe.LooseBounds.IsInside(PtPosition)) { return false; }

	const double PtDistance = FVector::DistSquared(Probe.Origin, PtPosition);
	if (PtDistance > Probe.MaxDistance) { return false; }
	if (PtDistance > Probe.BestCandidate.Distance) { return false; }

	const double Dot = Probe.Direction.Dot((PtPosition - Probe.Origin).GetSafeNormal());
	if (Dot < Probe.DotThreshold) { return false; }

	Probe.BestCandidate.Dot = Dot;
	Probe.BestCandidate.Distance = PtDistance;

	Probe.BestCandidate.Index = Index;
	Probe.BestCandidate.EntryKey = Point->MetadataEntry;

	return true;
}

void UPCGExGraphSolver::ResolveProbe(PCGExGraph::FSocketProbe& Probe) const
{
}

double UPCGExGraphSolver::PrepareProbesForPoint(
	TArray<PCGExGraph::FSocketInfos>& SocketInfos,
	const FPCGPoint& Point,
	TArray<PCGExGraph::FSocketProbe>& OutProbes) const
{
	OutProbes.Reset(SocketInfos.Num());
	double MaxDistance = 0.0;
	for (PCGExGraph::FSocketInfos& CurrentSocketInfos : SocketInfos)
	{
		PCGExGraph::FSocketProbe& NewProbe = OutProbes.Emplace_GetRef();
		InitializeProbe(NewProbe);
		NewProbe.SocketInfos = &CurrentSocketInfos;
		const double Dist = PrepareProbeForPointSocketPair(Point, NewProbe, CurrentSocketInfos);
		MaxDistance = FMath::Max(MaxDistance, Dist);
	}
	return MaxDistance;
}

double UPCGExGraphSolver::PrepareProbeForPointSocketPair(
	const FPCGPoint& Point,
	PCGExGraph::FSocketProbe& Probe,
	PCGExGraph::FSocketInfos InSocketInfos) const
{
	const FPCGExSocketAngle& BaseAngle = InSocketInfos.Socket->Descriptor.Angle;

	FVector Direction = BaseAngle.Direction;
	double DotTolerance = BaseAngle.DotThreshold;
	double MaxDistance = BaseAngle.MaxDistance;

	const FTransform PtTransform = Point.Transform;
	FVector Origin = PtTransform.GetLocation();

	if (InSocketInfos.Socket->Descriptor.bRelativeOrientation)
	{
		Direction = PtTransform.Rotator().RotateVector(Direction);
	}

	Direction.Normalize();

	if (InSocketInfos.Modifier &&
		InSocketInfos.Modifier->bEnabled &&
		InSocketInfos.Modifier->bValid)
	{
		MaxDistance *= InSocketInfos.Modifier->GetValue(Point);
	}

	if (InSocketInfos.LocalDirection &&
		InSocketInfos.LocalDirection->bEnabled &&
		InSocketInfos.LocalDirection->bValid)
	{
		// TODO: Apply LocalDirection
	}

	//TODO: Offset origin by extent?

	Probe.Direction = Direction;
	Probe.DotThreshold = DotTolerance;
	Probe.MaxDistance = MaxDistance * MaxDistance;
	Probe.DotOverDistanceCurve = BaseAngle.DotOverDistanceCurve;

	Direction.Normalize();
	FVector Offset = FVector::ZeroVector;
	switch (InSocketInfos.Socket->Descriptor.OffsetOrigin)
	{
	default:
	case EPCGExExtension::None:
		break;
	case EPCGExExtension::Extents:
		Offset = Direction * Point.GetExtents();
		Origin += Offset;
		break;
	case EPCGExExtension::Scale:
		Offset = Direction * Point.Transform.GetScale3D();
		Origin += Offset;
		break;
	case EPCGExExtension::ScaledExtents:
		Offset = Direction * Point.GetScaledExtents();
		Origin += Offset;
		break;
	}

	Probe.Origin = Origin;
	MaxDistance += Offset.Length();

	if (DotTolerance >= 0) { Probe.LooseBounds = PCGExMath::ConeBox(Probe.Origin, Direction, MaxDistance); }
	else { Probe.LooseBounds = FBoxCenterAndExtent(Probe.Origin, FVector(MaxDistance)).GetBox(); }

	return MaxDistance;
}
