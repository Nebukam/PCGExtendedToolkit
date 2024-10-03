// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExOrient.h"


#include "Paths/Orient/PCGExOrientAverage.h"

#define LOCTEXT_NAMESPACE "PCGExOrientElement"
#define PCGEX_NAMESPACE Orient

PCGEX_INITIALIZE_ELEMENT(Orient)

bool FPCGExOrientElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(Orient)

	if (!Settings->Orientation)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Please select an orientation module in the detail panel."));
		return false;
	}

	if (Settings->Output == EPCGExOrientUsage::OutputToAttribute)
	{
		PCGEX_VALIDATE_NAME(Settings->OutputAttribute);
	}

	if (Settings->bOutputDot)
	{
		PCGEX_VALIDATE_NAME(Settings->DotAttribute);
	}

	PCGEX_OPERATION_BIND(Orientation, UPCGExOrientOperation)
	Context->Orientation->OrientAxis = Settings->OrientAxis;
	Context->Orientation->UpAxis = Settings->UpAxis;

	return true;
}

bool FPCGExOrientElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExOrientElement::Execute);

	PCGEX_CONTEXT(Orient)
	PCGEX_EXECUTION_CHECK

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		bool bInvalidInputs = false;

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExOrient::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bInvalidInputs = true;
					Entry->InitializeOutput(Context, PCGExData::EInit::Forward);
					return false;
				}
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExOrient::FProcessor>>& NewBatch)
			{
				NewBatch->PrimaryOperation = Context->Orientation;
			}))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any paths to orient."));
			return true;
		}

		if (bInvalidInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 2 points and won't be processed."));
		}
	}

	if (!Context->ProcessPointsBatch(PCGExMT::State_Done)) { return false; }

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExOrient
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExOrient::Process);

		DefaultPointFilterValue = Settings->bFlipDirection;

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		LastIndex = PointDataFacade->GetNum() - 1;
		Orient = Cast<UPCGExOrientOperation>(PrimaryOperation);
		Orient->bClosedLoop = Context->ClosedLoop.IsClosedLoop(PointDataFacade->Source);
		if (!Orient->PrepareForData(PointDataFacade)) { return false; }

		if (Settings->Output == EPCGExOrientUsage::OutputToAttribute)
		{
			TransformWriter = PointDataFacade->GetWritable<FTransform>(Settings->OutputAttribute, false);
		}

		if (Settings->bOutputDot)
		{
			DotWriter = PointDataFacade->GetWritable<double>(Settings->DotAttribute, false);
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);
		FilterScope(StartIndex, Count);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		FTransform OutT;

		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;

		const PCGExData::FPointRef Current = PointIO->GetOutPointRef(Index);
		if (Orient->bClosedLoop)
		{
			const PCGExData::FPointRef Previous = Index == 0 ? PointIO->GetInPointRef(LastIndex) : PointIO->GetInPointRef(Index - 1);
			const PCGExData::FPointRef Next = Index == LastIndex ? PointIO->GetInPointRef(0) : PointIO->GetInPointRef(Index + 1);
			OutT = Orient->ComputeOrientation(Current, Previous, Next, PointFilterCache[Index] ? -1 : 1);
			if (Settings->bOutputDot) { DotWriter->GetMutable(Index) = DotProduct(Current, Previous, Next); }
		}
		else
		{
			const PCGExData::FPointRef Previous = Index == 0 ? Current : PointIO->GetInPointRef(Index - 1);
			const PCGExData::FPointRef Next = Index == LastIndex ? PointIO->GetInPointRef(LastIndex) : PointIO->GetInPointRef(Index + 1);
			OutT = Orient->ComputeOrientation(Current, Previous, Next, PointFilterCache[Index] ? -1 : 1);
			if (Settings->bOutputDot) { DotWriter->GetMutable(Index) = DotProduct(Current, Previous, Next); }
		}

		if (TransformWriter) { TransformWriter->GetMutable(Index) = OutT; }
		else { Point.Transform = OutT; }
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
