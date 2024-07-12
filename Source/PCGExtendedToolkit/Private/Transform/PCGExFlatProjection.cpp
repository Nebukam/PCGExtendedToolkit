// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/PCGExFlatProjection.h"

#define LOCTEXT_NAMESPACE "PCGExFlatProjectionElement"
#define PCGEX_NAMESPACE FlatProjection

PCGExData::EInit UPCGExFlatProjectionSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(FlatProjection)

FPCGExFlatProjectionContext::~FPCGExFlatProjectionContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExFlatProjectionElement::Boot(FPCGContext* InContext) const
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

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		bool bHasInvalidEntries = false;

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExFlatProjection::FProcessor>>(
			[&](PCGExData::FPointIO* Entry)
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
			[&](PCGExPointsMT::TBatch<PCGExFlatProjection::FProcessor>* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any points to process."));
			return true;
		}

		if (bHasInvalidEntries)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some points are missing the required attributes."));
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->OutputMainPoints();

	return Context->TryComplete();
}

namespace PCGExFlatProjection
{
	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFlatProjection::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FlatProjection)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		bWriteAttribute = Settings->bSaveAttributeForRestore;
		bInverseExistingProjection = Settings->bRestorePreviousProjection;
		bProjectLocalTransform = Settings->bAlignLocalTransform;

		if (bInverseExistingProjection)
		{
			TransformReader = PointDataFacade->GetOrCreateReader<FTransform>(TypedContext->CachedTransformAttributeName);
		}
		else if (bWriteAttribute)
		{
			ProjectionDetails = Settings->ProjectionDetails;
			ProjectionDetails.Init(Context, PointDataFacade);
			TransformWriter = PointDataFacade->GetOrCreateWriter<FTransform>(TypedContext->CachedTransformAttributeName, true);
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		if (bInverseExistingProjection)
		{
			Point.Transform = TransformReader->Values[Index];
		}
		else if (bWriteAttribute)
		{
			TransformWriter->Values[Index] = Point.Transform;

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
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FlatProjection)
		if (bInverseExistingProjection)
		{
			UPCGMetadata* Metadata = PointIO->GetOut()->Metadata;
			Metadata->DeleteAttribute(TypedContext->CachedTransformAttributeName);
		}
		else if (bWriteAttribute)
		{
			PointDataFacade->Write(AsyncManagerPtr, true);
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
