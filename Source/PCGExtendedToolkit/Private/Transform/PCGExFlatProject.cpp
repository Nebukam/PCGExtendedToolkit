// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/PCGExFlatProject.h"

#define LOCTEXT_NAMESPACE "PCGExFlatProjectElement"
#define PCGEX_NAMESPACE FlatProject

PCGExData::EInit UPCGExFlatProjectSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(FlatProject)

FPCGExFlatProjectContext::~FPCGExFlatProjectContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExFlatProjectElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FlatProject)

	PCGEX_VALIDATE_NAME(Settings->AttributePrefix)

	Context->CachedTransformAttributeName = PCGEx::MakePCGExAttributeName(Settings->AttributePrefix.ToString(), TEXT("T"));

	return true;
}

bool FPCGExFlatProjectElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFlatProjectElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FlatProject)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		bool bHasInvalidEntries = false;

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExFlatProject::FProcessor>>(
			[&](PCGExData::FPointIO* Entry)
			{
				if (Settings->bInverseExistingProjection)
				{
					if (!Entry->GetIn()->Metadata->HasAttribute(Context->CachedTransformAttributeName))
					{
						bHasInvalidEntries = true;
						return false;
					}
				}
				return true;
			},
			[&](PCGExPointsMT::TBatch<PCGExFlatProject::FProcessor>* NewBatch)
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

namespace PCGExFlatProject
{
	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FlatProject)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		bInverseExistingProjection = Settings->bInverseExistingProjection;
		bProjectLocalTransform = Settings->bAlignLocalTransform;

		if (bInverseExistingProjection)
		{
			TransformReader = PointDataCache->GetOrCreateReader<FTransform>(TypedContext->CachedTransformAttributeName);
		}
		else
		{
			ProjectionSettings = Settings->ProjectionSettings;
			ProjectionSettings.Init(Context, PointDataCache);
			TransformWriter = PointDataCache->GetOrCreateWriter<FTransform>(TypedContext->CachedTransformAttributeName, true);
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
		else
		{
			TransformWriter->Values[Index] = Point.Transform;

			if (bProjectLocalTransform)
			{
				Point.Transform = ProjectionSettings.ProjectFlat(Point.Transform);
			}
			else
			{
				Point.Transform.SetLocation(ProjectionSettings.ProjectFlat(Point.Transform.GetLocation(), Index));
			}
		}
	}

	void FProcessor::CompleteWork()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FlatProject)
		if (bInverseExistingProjection)
		{
			UPCGMetadata* Metadata = PointIO->GetOut()->Metadata;
			Metadata->DeleteAttribute(TypedContext->CachedTransformAttributeName);
		}
		else
		{
			PointDataCache->Write(AsyncManagerPtr, true);
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
