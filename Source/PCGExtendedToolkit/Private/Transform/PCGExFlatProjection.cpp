// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/PCGExFlatProjection.h"


#define LOCTEXT_NAMESPACE "PCGExFlatProjectionElement"
#define PCGEX_NAMESPACE FlatProjection

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
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFlatProjection::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)
		PointDataFacade->GetOut()->AllocateProperties(EPCGPointNativeProperties::Transform);

		bWriteAttribute = Settings->bSaveAttributeForRestore;
		bInverseExistingProjection = Settings->bRestorePreviousProjection;
		bProjectLocalTransform = Settings->bAlignLocalTransform;

		if (bInverseExistingProjection)
		{
			TransformReader = PointDataFacade->GetReadable<FTransform>(Context->CachedTransformAttributeName, PCGExData::EIOSide::In, true);
		}
		else if (bWriteAttribute)
		{
			ProjectionDetails = Settings->ProjectionDetails;
			ProjectionDetails.Init(ExecutionContext, PointDataFacade);
			TransformWriter = PointDataFacade->GetWritable<FTransform>(Context->CachedTransformAttributeName, PCGExData::EBufferInit::New);
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::FlatProjection::ProcessPoints);

		PointDataFacade->Fetch(Scope);

		TPCGValueRange<FTransform> OutTransforms = PointDataFacade->GetOut()->GetTransformValueRange(false);

		if (bInverseExistingProjection)
		{
			PCGEX_SCOPE_LOOP(Index) { OutTransforms[Index] = TransformReader->Read(Index); }
		}
		else if (bWriteAttribute)
		{
			if (bWriteAttribute)
			{
				PCGEX_SCOPE_LOOP(Index) { TransformWriter->SetValue(Index, OutTransforms[Index]); }
			}

			if (bProjectLocalTransform)
			{
				PCGEX_SCOPE_LOOP(Index) { OutTransforms[Index] = ProjectionDetails.ProjectFlat(OutTransforms[Index]); }
			}
			else
			{
				PCGEX_SCOPE_LOOP(Index) { OutTransforms[Index].SetLocation(ProjectionDetails.ProjectFlat(OutTransforms[Index].GetLocation(), Index)); }
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
