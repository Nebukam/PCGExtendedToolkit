// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExEdgeDirectionSettings.h"

#include "Core/PCGExContext.h"
#include "Data/PCGExDataPreloader.h"
#include "PCGExSorting.h"
#include "Data/PCGExData.h"
#include "Graph/PCGExCluster.h"

void FPCGExEdgeDirectionSettings::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader, const TArray<FPCGExSortRuleConfig>* InSortingRules) const
{
	if (DirectionMethod == EPCGExEdgeDirectionMethod::EndpointsSort)
	{
		FacadePreloader.Register<double>(InContext, DirSourceAttribute);
	}
}

bool FPCGExEdgeDirectionSettings::Init(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TArray<FPCGExSortRuleConfig>* InSortingRules, const bool bQuiet)
{
	bAscendingDesired = DirectionChoice == EPCGExEdgeDirectionChoice::SmallestToGreatest;
	if (DirectionMethod == EPCGExEdgeDirectionMethod::EndpointsSort)
	{
		if (!InSortingRules) { return false; }

		Sorter = MakeShared<PCGExSorting::FPointSorter>(InContext, InVtxDataFacade, *InSortingRules);
		Sorter->SortDirection = DirectionChoice == EPCGExEdgeDirectionChoice::GreatestToSmallest ? EPCGExSortDirection::Descending : EPCGExSortDirection::Ascending;
		if (!Sorter->Init(InContext)) { return false; }
	}
	return true;
}

bool FPCGExEdgeDirectionSettings::InitFromParent(FPCGExContext* InContext, const FPCGExEdgeDirectionSettings& ParentSettings, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade, const bool bQuiet)
{
	DirectionMethod = ParentSettings.DirectionMethod;
	DirectionChoice = ParentSettings.DirectionChoice;

	bAscendingDesired = ParentSettings.bAscendingDesired;
	Sorter = ParentSettings.Sorter;

	if (DirectionMethod == EPCGExEdgeDirectionMethod::EdgeDotAttribute)
	{
		EdgeDirReader = InEdgeDataFacade->GetBroadcaster<FVector>(DirSourceAttribute, true);
		if (!EdgeDirReader)
		{
			if (!bQuiet) { PCGEX_LOG_INVALID_SELECTOR_C(InContext, Dir Source (Edges), DirSourceAttribute) }
			return false;
		}
	}

	return true;
}

bool FPCGExEdgeDirectionSettings::SortEndpoints(const PCGExCluster::FCluster* InCluster, PCGExGraph::FEdge& InEdge) const
{
	const uint32 Start = InEdge.Start;
	const uint32 End = InEdge.End;

	bool bAscending = true;

	if (DirectionMethod == EPCGExEdgeDirectionMethod::EndpointsOrder)
	{
	}
	else if (DirectionMethod == EPCGExEdgeDirectionMethod::EndpointsIndices)
	{
		bAscending = (Start < End);
	}
	else if (DirectionMethod == EPCGExEdgeDirectionMethod::EndpointsSort)
	{
		bAscending = Sorter->Sort(Start, End);
	}
	else if (DirectionMethod == EPCGExEdgeDirectionMethod::EdgeDotAttribute && InEdge.Index != -1)
	{
		const FVector A = InCluster->VtxPoints->GetTransform(Start).GetLocation();
		const FVector B = InCluster->VtxPoints->GetTransform(End).GetLocation();

		const FVector& EdgeDir = (A - B).GetSafeNormal();
		const FVector& CounterDir = EdgeDirReader->Read(InEdge.Index);
		bAscending = CounterDir.Dot(EdgeDir * -1) < CounterDir.Dot(EdgeDir); // TODO : Do we really need both dots?
	}

	if (bAscending != bAscendingDesired)
	{
		InEdge.Start = End;
		InEdge.End = Start;
		return true;
	}

	return false;
}

bool FPCGExEdgeDirectionSettings::SortExtrapolation(const PCGExCluster::FCluster* InCluster, const int32 InEdgeIndex, const int32 StartNodeIndex, const int32 EndNodeIndex) const
{
	PCGExGraph::FEdge ChainDir = PCGExGraph::FEdge(InEdgeIndex, InCluster->GetNodePointIndex(StartNodeIndex), InCluster->GetNode(EndNodeIndex)->PointIndex);
	return SortEndpoints(InCluster, ChainDir);
}
