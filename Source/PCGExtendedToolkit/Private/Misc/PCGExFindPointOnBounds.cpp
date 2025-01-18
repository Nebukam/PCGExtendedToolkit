// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExFindPointOnBounds.h"

#include "Data/PCGExData.h"

#define LOCTEXT_NAMESPACE "PCGExFindPointOnBoundsElement"
#define PCGEX_NAMESPACE FindPointOnBounds

PCGExData::EIOInit UPCGExFindPointOnBoundsSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::New; }

PCGEX_INITIALIZE_ELEMENT(FindPointOnBounds)

bool FPCGExFindPointOnBoundsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FindPointOnBounds)

	PCGEX_FWD(CarryOverDetails)
	Context->CarryOverDetails.Init();

	if (Settings->OutputMode == EPCGExPointOnBoundsOutputMode::Merged)
	{
		TSet<FName> AttributeMismatches;

		Context->BestIndices.Init(-1, Context->MainPoints->Num());

		Context->MergedOut = PCGExData::NewPointIO(Context, Settings->GetMainOutputPin(), 0);
		Context->MergedAttributesInfos = PCGEx::FAttributesInfos::Get(Context->MainPoints, AttributeMismatches);

		Context->CarryOverDetails.Attributes.Prune(*Context->MergedAttributesInfos);
		Context->CarryOverDetails.Attributes.Prune(AttributeMismatches);

		Context->MergedOut->InitializeOutput(PCGExData::EIOInit::New);
		Context->MergedOut->GetOut()->GetMutablePoints().SetNum(Context->MainPoints->Num());
		Context->MergedOut->GetOutKeys(true);

		if (!AttributeMismatches.IsEmpty() && !Settings->bQuietAttributeMismatchWarning)
		{
			PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("Some attributes on incoming data share the same name but not the same type. Whatever type was discovered first will be used."));
		}
	}

	return true;
}

bool FPCGExFindPointOnBoundsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFindPointOnBoundsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FindPointOnBounds)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExFindPointOnBounds::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExFindPointOnBounds::FProcessor>>& NewBatch)
			{
				//NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	if (Settings->OutputMode == EPCGExPointOnBoundsOutputMode::Merged)
	{
		PCGExFindPointOnBounds::MergeBestCandidatesAttributes(
			Context->MergedOut,
			Context->MainPoints->Pairs,
			Context->BestIndices,
			*Context->MergedAttributesInfos);

		Context->MergedOut->StageOutput();
	}
	else
	{
		Context->MainPoints->StageOutputs();
	}

	return Context->TryComplete();
}

namespace PCGExFindPointOnBounds
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFindPointOnBounds::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		const FBox Bounds = PointDataFacade->Source->GetIn()->GetBounds();
		SearchPosition = Bounds.GetCenter() + Bounds.GetExtent() * Settings->UVW;

		StartParallelLoopForPoints(PCGExData::ESource::In);

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope)
	{
		if (const double Dist = FVector::Dist(Point.Transform.GetLocation(), SearchPosition); Dist < BestDistance)
		{
			FWriteScopeLock WriteLock(BestIndexLock);
			BestPosition = Point.Transform.GetLocation();
			BestIndex = Index;
			BestDistance = Dist;
		}
	}

	void FProcessor::CompleteWork()
	{
		const FVector Offset = (BestPosition - PointDataFacade->Source->GetIn()->GetBounds().GetCenter()).GetSafeNormal() * Settings->Offset;

		if (Settings->OutputMode == EPCGExPointOnBoundsOutputMode::Merged)
		{
			const PCGMetadataEntryKey OriginalKey = Context->MergedOut->GetOut()->GetMutablePoints()[PointDataFacade->Source->IOIndex].MetadataEntry;
			FPCGPoint& OutPoint = (Context->MergedOut->GetOut()->GetMutablePoints()[PointDataFacade->Source->IOIndex] = PointDataFacade->Source->GetInPoint(BestIndex));
			OutPoint.MetadataEntry = OriginalKey;
			OutPoint.Transform.AddToTranslation(Offset);
		}
		else
		{
			PCGEX_INIT_IO_VOID(PointDataFacade->Source, PCGExData::EIOInit::New)
			PointDataFacade->Source->GetOut()->GetMutablePoints().SetNum(1);

			FPCGPoint& OutPoint = (PointDataFacade->Source->GetOut()->GetMutablePoints()[0] = PointDataFacade->Source->GetInPoint(BestIndex));
			PointDataFacade->Source->GetOut()->Metadata->InitializeOnSet(OutPoint.MetadataEntry);
			OutPoint.Transform.AddToTranslation(Offset);
		}
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
