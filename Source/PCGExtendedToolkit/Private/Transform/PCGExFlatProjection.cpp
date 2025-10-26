﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/PCGExFlatProjection.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Sampling/PCGExSampling.h"


#define LOCTEXT_NAMESPACE "PCGExFlatProjectionElement"
#define PCGEX_NAMESPACE FlatProjection

PCGEX_INITIALIZE_ELEMENT(FlatProjection)
PCGEX_ELEMENT_BATCH_POINT_IMPL(FlatProjection)

bool FPCGExFlatProjectionElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FlatProjection)

	if (Settings->bRestorePreviousProjection)
	{
#define PCGEX_REGISTER_FLAG(_COMPONENT, _ARRAY) \
if ((Settings->_COMPONENT & static_cast<uint8>(EPCGExApplySampledComponentFlags::X)) != 0){ Context->_ARRAY.Add(0); Context->AppliedComponents++; } \
if ((Settings->_COMPONENT & static_cast<uint8>(EPCGExApplySampledComponentFlags::Y)) != 0){ Context->_ARRAY.Add(1); Context->AppliedComponents++; } \
if ((Settings->_COMPONENT & static_cast<uint8>(EPCGExApplySampledComponentFlags::Z)) != 0){ Context->_ARRAY.Add(2); Context->AppliedComponents++; }

		PCGEX_REGISTER_FLAG(TransformPosition, TrPosComponents)
		PCGEX_REGISTER_FLAG(TransformRotation, TrRotComponents)
		PCGEX_REGISTER_FLAG(TransformScale, TrScaComponents)

#undef PCGEX_REGISTER_FLAG
	}


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
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some points are missing the required attributes."))

		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Settings->bRestorePreviousProjection)
				{
					if (!Entry->GetIn()->Metadata->HasAttribute(Context->CachedTransformAttributeName))
					{
						bHasInvalidInputs = true;
						return false;
					}
				}
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to process."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExFlatProjection
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFlatProjection::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InAsyncManager)) { return false; }

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
			if (ProjectionDetails.Method == EPCGExProjectionMethod::Normal) { ProjectionDetails.Init(PointDataFacade); }
			else { ProjectionDetails.Init(PCGExGeo::FBestFitPlane(PointDataFacade->GetIn()->GetConstTransformValueRange())); }

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
			PCGEX_SCOPE_LOOP(Index)
			{
				const FTransform& CurrentTr = OutTransforms[Index];

				const FTransform& RestoreTr = TransformReader->Read(Index);
				FVector OutRotation = CurrentTr.GetRotation().Euler();
				FVector OutPosition = CurrentTr.GetLocation();
				FVector OutScale = CurrentTr.GetScale3D();

				const FVector InTrRot = RestoreTr.GetRotation().Euler();
				for (const int32 C : Context->TrRotComponents) { OutRotation[C] = InTrRot[C]; }

				FVector InTrPos = RestoreTr.GetLocation();
				for (const int32 C : Context->TrPosComponents) { OutPosition[C] = InTrPos[C]; }

				FVector InTrSca = RestoreTr.GetScale3D();
				for (const int32 C : Context->TrScaComponents) { OutScale[C] = InTrSca[C]; }

				OutTransforms[Index] = FTransform(FQuat::MakeFromEuler(OutRotation), OutPosition, OutScale);
			}
		}
		else if (bWriteAttribute)
		{
			if (bWriteAttribute)
			{
				PCGEX_SCOPE_LOOP(Index) { TransformWriter->SetValue(Index, OutTransforms[Index]); }
			}

			if (bProjectLocalTransform)
			{
				PCGEX_SCOPE_LOOP(Index) { OutTransforms[Index] = ProjectionDetails.ProjectFlat(OutTransforms[Index], Index); }
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
			PointDataFacade->WriteFastest(AsyncManager);
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
