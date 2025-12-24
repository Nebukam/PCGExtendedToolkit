// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExFusePoints.h"


#include "Data/PCGExPointIO.h"
#include "Blenders/PCGExUnionBlender.h"
#include "Async/ParallelFor.h"
#include "Clusters/PCGExClusterCommon.h"
#include "Data/PCGExData.h"
#include "PCGExGraphs/Public/Graphs/Union/PCGExIntersections.h"

#define LOCTEXT_NAMESPACE "PCGExFusePointsElement"
#define PCGEX_NAMESPACE FusePoints

namespace PCGExFuse
{
	FFusedPoint::FFusedPoint(const int32 InIndex, const FVector& InPosition)
		: Index(InIndex), Position(InPosition)
	{
	}

	void FFusedPoint::Add(const int32 InIndex, const double Distance)
	{
		FWriteScopeLock WriteLock(IndicesLock);
		Fused.Add(InIndex);
		Distances.Add(Distance);
		MaxDistance = FMath::Max(MaxDistance, Distance);
	}
}

PCGEX_INITIALIZE_ELEMENT(FusePoints)
PCGEX_ELEMENT_BATCH_POINT_IMPL(FusePoints)

bool FPCGExFusePointsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FusePoints)

	Context->Distances = Settings->PointPointIntersectionDetails.FuseDetails.GetDistances();
	if (!Settings->PointPointIntersectionDetails.SanityCheck(Context)) { return false; }

	PCGEX_FWD(CarryOverDetails)
	Context->CarryOverDetails.Init();

	return true;
}

bool FPCGExFusePointsElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFusePointsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FusePoints)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints([&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; }, [&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
		{
			NewBatch->bRequiresWriteStep = Settings->Mode != EPCGExFusedPointOutput::MostCentral;
		}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to fuse."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExFusePoints
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFusePoints::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InTaskManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::New)

		UnionGraph = MakeShared<PCGExGraphs::FUnionGraph>(Settings->PointPointIntersectionDetails.FuseDetails, PointDataFacade->GetIn()->GetBounds().ExpandBy(10), Context->MainPoints);

		// TODO : See if we can support scoped get
		if (!UnionGraph->Init(Context, PointDataFacade, false)) { return false; }
		UnionGraph->Reserve(PointDataFacade->GetNum(), 0);

		// Register fetch-able buffers for chunked reads
		TArray<PCGExData::FAttributeIdentity> SourceAttributes;
		PCGExBlending::GetFilteredIdentities(PointDataFacade->GetIn()->Metadata, SourceAttributes, &Settings->BlendingDetails, &Context->CarryOverDetails);

		PointDataFacade->CreateReadables(SourceAttributes);

		bForceSingleThreadedProcessPoints = Settings->PointPointIntersectionDetails.FuseDetails.DoInlineInsertion();
		StartParallelLoopForPoints(PCGExData::EIOSide::In);

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::FusePoints::ProcessPoints);

		PointDataFacade->Fetch(Scope);

		if (bForceSingleThreadedProcessPoints)
		{
			PCGEX_SCOPE_LOOP(Index) { UnionGraph->InsertPoint_Unsafe(PointDataFacade->GetInPoint(Index)); }
		}
		else
		{
			PCGEX_SCOPE_LOOP(Index) { UnionGraph->InsertPoint(PointDataFacade->GetInPoint(Index)); }
		}
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		TPCGValueRange<FTransform> Transforms = PointDataFacade->GetOut()->GetTransformValueRange(false);

		TArray<int32> ReadIndices;
		TArray<int32> WriteIndices;

		ReadIndices.SetNumUninitialized(Scope.Count);
		WriteIndices.SetNumUninitialized(Scope.Count);

		for (int i = 0; i < Scope.Count; ++i)
		{
			const int32 Idx = Scope.Start + i;
			ReadIndices[i] = UnionGraph->Nodes[Idx]->Point.Index;
			WriteIndices[i] = Idx;
		}

		PointDataFacade->Source->InheritProperties(ReadIndices, WriteIndices, PointDataFacade->GetAllocations() & ~EPCGPointNativeProperties::MetadataEntry);

		TArray<PCGExData::FWeightedPoint> WeightedPoints;
		TArray<PCGEx::FOpStats> Trackers;
		UnionBlender->InitTrackers(Trackers);

		bool bUpdateCenter = Settings->BlendingDetails.PropertiesOverrides.bOverridePosition && Settings->BlendingDetails.PropertiesOverrides.PositionBlending == EPCGExBlendingType::None;

		PCGEX_SHARED_CONTEXT_VOID(Context->GetOrCreateHandle())

		PCGEX_SCOPE_LOOP(Index)
		{
			const FVector Center = UnionGraph->Nodes[Index]->UpdateCenter(UnionGraph->NodesUnion, Context->MainPoints);

			if (bUpdateCenter) { Transforms[Index].SetLocation(Center); }

			UnionBlender->MergeSingle(Index, WeightedPoints, Trackers);
			if (IsUnionWriter) { IsUnionWriter->SetValue(Index, WeightedPoints.Num() > 1); }
			if (UnionSizeWriter) { UnionSizeWriter->SetValue(Index, WeightedPoints.Num()); }
		}
	}

	void FProcessor::CompleteWork()
	{
		const int32 NumUnionNodes = UnionGraph->Nodes.Num();

		UPCGBasePointData* OutData = PointDataFacade->GetOut();
		PCGExPointArrayDataHelpers::SetNumPointsAllocated(OutData, NumUnionNodes, PointDataFacade->GetAllocations());

		if (Settings->Mode == EPCGExFusedPointOutput::MostCentral)
		{
			TArray<int32>& IdxMapping = PointDataFacade->Source->GetIdxMapping(NumUnionNodes);
			const PCGPointOctree::FPointOctree& Octree = PointDataFacade->GetIn()->GetPointOctree();
			const TConstPCGValueRange<FTransform> OutTransforms = PointDataFacade->GetIn()->GetConstTransformValueRange();
			ParallelFor(NumUnionNodes, [&](int32 Index)
			{
				const FVector Center = UnionGraph->Nodes[Index]->UpdateCenter(UnionGraph->NodesUnion, Context->MainPoints);
				double BestDist = MAX_dbl;
				int32 BestIndex = -1;

				Octree.FindNearbyElements(Center, [&](const PCGPointOctree::FPointRef& PointRef)
				{
					const double Dist = FVector::DistSquared(Center, OutTransforms[PointRef.Index].GetLocation());
					if (Dist < BestDist)
					{
						BestDist = Dist;
						BestIndex = PointRef.Index;
					}
				});

				if (BestIndex == -1) { BestIndex = UnionGraph->Nodes[Index]->Point.Index; }
				IdxMapping[Index] = BestIndex;
			});

			PointDataFacade->Source->ConsumeIdxMapping(PointDataFacade->GetAllocations());

			return;
		}

		const TSharedPtr<PCGExBlending::FUnionBlender> TypedBlender = MakeShared<PCGExBlending::FUnionBlender>(
			const_cast<FPCGExBlendingDetails*>(&Settings->BlendingDetails), &Context->CarryOverDetails, Context->Distances);
		UnionBlender = TypedBlender;

		TArray<TSharedRef<PCGExData::FFacade>> UnionSources;
		UnionSources.Add(PointDataFacade);

		TypedBlender->AddSources(UnionSources, &PCGExClusters::Labels::ProtectedClusterAttributes);
		if (!TypedBlender->Init(Context, PointDataFacade, UnionGraph->NodesUnion))
		{
			bIsProcessorValid = false;
			return;
		}

		// Initialize writables AFTER we initialize Union Blender, so these don't get captured in the mix

		const FPCGExPointUnionMetadataDetails& PtUnionDetails = Settings->PointPointIntersectionDetails.PointUnionData;

		if (PtUnionDetails.bWriteIsUnion)
		{
			IsUnionWriter = PointDataFacade->GetWritable<bool>(PtUnionDetails.IsUnionAttributeName, false, true, PCGExData::EBufferInit::New);
		}

		if (PtUnionDetails.bWriteUnionSize)
		{
			UnionSizeWriter = PointDataFacade->GetWritable<int32>(PtUnionDetails.UnionSizeAttributeName, 1, true, PCGExData::EBufferInit::New);
		}

		StartParallelLoopForRange(NumUnionNodes);
	}

	void FProcessor::Write()
	{
		PointDataFacade->WriteFastest(TaskManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
