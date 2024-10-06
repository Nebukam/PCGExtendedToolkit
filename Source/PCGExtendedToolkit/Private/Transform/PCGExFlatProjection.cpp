// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/PCGExFlatProjection.h"


#define LOCTEXT_NAMESPACE "PCGExFlatProjectionElement"
#define PCGEX_NAMESPACE FlatProjection

PCGExData::EInit UPCGExFlatProjectionSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(FlatProjection)

bool FPCGExFlatProjectionElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FlatProjection)

	if (Settings->bSaveAttributeForRestore || Settings->bRestorePreviousProjection)
	{
		PCGEX_VALIDATE_NAME(Settings->AttributePrefix)
		Context->CachedTransformAttributeName = PCGEx::MakePCGExAttributeName(Settings->AttributePrefix.ToString(), TEXT("T"));
	}

	return true;
}

bool FPCGExFlatProjectionElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFlatProjectionElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FlatProjection)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		bool bHasInvalidEntries = false;

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExFlatProjection::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Settings->bRestorePreviousProjection)
				{
					if (!Entry->GetIn()->Metadata->HasAttribute(Context->CachedTransformAttributeName))
					{
						bHasInvalidEntries = true;
						return false;
					}
				}
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExFlatProjection::FProcessor>>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to process."));
		}

		if (bHasInvalidEntries)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some points are missing the required attributes."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExFlatProjection
{
	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFlatProjection::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		bWriteAttribute = Settings->bSaveAttributeForRestore;
		bInverseExistingProjection = Settings->bRestorePreviousProjection;
		bProjectLocalTransform = Settings->bAlignLocalTransform;

		if (bInverseExistingProjection)
		{
			TransformReader = PointDataFacade->GetScopedReadable<FTransform>(Context->CachedTransformAttributeName);
		}
		else if (bWriteAttribute)
		{
			ProjectionDetails = Settings->ProjectionDetails;
			ProjectionDetails.Init(ExecutionContext, PointDataFacade);
			TransformWriter = PointDataFacade->GetWritable<FTransform>(Context->CachedTransformAttributeName, true);
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		if (bInverseExistingProjection)
		{
			Point.Transform = TransformReader->Read(Index);
		}
		else if (bWriteAttribute)
		{
			TransformWriter->GetMutable(Index) = Point.Transform;

			if (bProjectLocalTransform)
			{
				Point.Transform = ProjectionDetails.ProjectFlat(Point.Transform);
			}
			else
			{
				Point.Transform.SetLocation(ProjectionDetails.ProjectFlat(Point.Transform.GetLocation(), Index));
			}
		}
	}

	void FProcessor::CompleteWork()
	{
		if (bInverseExistingProjection)
		{
			PointDataFacade->Source->DeleteAttribute(Context->CachedTransformAttributeName);
		}
		else if (bWriteAttribute)
		{
			PointDataFacade->Write(AsyncManager);
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
