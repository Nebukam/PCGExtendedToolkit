// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExFusePoints.h"

#include "Data/Blending/PCGExUnionBlender.h"


#include "Graph/PCGExIntersections.h"

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

bool FPCGExFusePointsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FusePoints)

	Context->Distances = PCGExDetails::MakeDistances(Settings->PointPointIntersectionDetails.FuseDetails.SourceDistance, Settings->PointPointIntersectionDetails.FuseDetails.TargetDistance);

	if (!Settings->PointPointIntersectionDetails.SanityCheck(Context)) { return false; }

	PCGEX_FWD(CarryOverDetails)
	Context->CarryOverDetails.Init();

	return true;
}

bool FPCGExFusePointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFusePointsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FusePoints)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExFusePoints::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExFusePoints::FProcessor>>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to fuse."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExFusePoints
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFusePoints::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::New)

		UnionGraph = MakeShared<PCGExGraph::FUnionGraph>(
			Settings->PointPointIntersectionDetails.FuseDetails,
			PointDataFacade->GetIn()->GetBounds().ExpandBy(10));

		// TODO : See if we can support scoped get
		if (!UnionGraph->Init(Context, PointDataFacade, false)) { return false; }

		// Register fetch-able buffers for chunked reads
		TArray<PCGEx::FAttributeIdentity> SourceAttributes;
		PCGExDataBlending::GetFilteredIdentities(
			PointDataFacade->GetIn()->Metadata, SourceAttributes,
			&Settings->BlendingDetails, &Context->CarryOverDetails);

		PointDataFacade->CreateReadables(SourceAttributes);

		bDaisyChainProcessPoints = Settings->PointPointIntersectionDetails.FuseDetails.DoInlineInsertion();
		StartParallelLoopForPoints(PCGExData::EIOSide::In);

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::FusePoints::ProcessPoints);

		PointDataFacade->Fetch(Scope);

		PCGEX_SCOPE_LOOP(Index) { UnionGraph->InsertPoint(PointDataFacade->GetInPoint(Index)); }
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

		PCGEX_SCOPE_LOOP(Index)
		{
			Transforms[Index].SetLocation(UnionGraph->Nodes[Index]->UpdateCenter(UnionGraph->NodesUnion, Context->MainPoints));
			UnionBlender->MergeSingle(Index, WeightedPoints, Trackers);
			if (IsUnionWriter) { IsUnionWriter->SetValue(Index, WeightedPoints.Num() > 1); }
			if (UnionSizeWriter) { UnionSizeWriter->SetValue(Index, WeightedPoints.Num()); }
		}
	}

	void FProcessor::CompleteWork()
	{
		const int32 NumUnionNodes = UnionGraph->Nodes.Num();

		UPCGBasePointData* OutData = PointDataFacade->GetOut();
		PCGEx::SetNumPointsAllocated(OutData, NumUnionNodes, PointDataFacade->GetAllocations());

		const TSharedPtr<PCGExDataBlending::FUnionBlender> TypedBlender = MakeShared<PCGExDataBlending::FUnionBlender>(const_cast<FPCGExBlendingDetails*>(&Settings->BlendingDetails), &Context->CarryOverDetails, Context->Distances);
		UnionBlender = TypedBlender;
		
		TArray<TSharedRef<PCGExData::FFacade>> UnionSources;
		UnionSources.Add(PointDataFacade);

		TypedBlender->AddSources(UnionSources, &PCGExGraph::ProtectedClusterAttributes);
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
		PointDataFacade->WriteFastest(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
