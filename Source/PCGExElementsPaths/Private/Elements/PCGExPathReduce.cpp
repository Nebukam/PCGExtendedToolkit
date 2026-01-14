// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExPathReduce.h"

#include "Animation/AnimInstanceProxy.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Data/PCGTextureData.h"
#include "Details/PCGExSettingsDetails.h"
#include "Helpers/PCGExPathSimplifier.h"
#include "Paths/PCGExPath.h"
#include "Paths/PCGExPathsHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExPathReduceElement"
#define PCGEX_NAMESPACE PathReduce

PCGEX_INITIALIZE_ELEMENT(PathReduce)

PCGExData::EIOInit UPCGExPathReduceSettings::GetMainDataInitializationPolicy() const { return PCGExData::EIOInit::Duplicate; }

PCGEX_ELEMENT_BATCH_POINT_IMPL(PathReduce)

bool FPCGExPathReduceElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathReduce)

	return true;
}

bool FPCGExPathReduceElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathReduceElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathReduce)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some input have less than 2 points and will be ignored."))
		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				PCGEX_SKIP_INVALID_PATH_ENTRY
				return true;
			}, [&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any valid path."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExPathReduce
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPathReduce::Process);

		if (!IProcessor::Process(InTaskManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		bClosedLoop = PCGExPaths::Helpers::GetClosedLoop(PointDataFacade->GetIn());

		FilterAll();

		for (int8& Pass : PointFilterCache) { Pass = !Pass; }

		SmoothingGetter = Settings->Smoothing.GetValueSetting();
		if (!SmoothingGetter->Init(PointDataFacade, false)) { return false; }

		TArray<PCGExPaths::FSimplifiedPoint> SimplifiedResult;

		if (SmoothingGetter->IsConstant())
		{
			SimplifiedResult = PCGExPaths::FCurveSimplifier::SimplifyPolyline(
				PointDataFacade->GetIn()->GetConstTransformValueRange(),
				PointFilterCache,
				Settings->ErrorTolerance,
				bClosedLoop,
				SmoothingGetter->Read(0),
				Settings->SmoothingMode
			);
		}
		else
		{
			TArray<double> SmoothingValues;
			SmoothingValues.SetNumUninitialized(PointDataFacade->GetNum());
			SmoothingGetter->ReadScope(0, SmoothingValues);
			
			SimplifiedResult = PCGExPaths::FCurveSimplifier::SimplifyPolyline(
				PointDataFacade->GetIn()->GetConstTransformValueRange(),
				PointFilterCache,
				SmoothingValues,
				Settings->ErrorTolerance,
				bClosedLoop,
				Settings->SmoothingMode
			);
		}

		Mask.Init(true, PointDataFacade->GetNum());

		TPCGValueRange<FTransform> OutTransforms = PointDataFacade->GetOut()->GetTransformValueRange();

		ArriveWriter = PointDataFacade->GetWritable<FVector>(Settings->ArriveName, PCGExData::EBufferInit::Inherit);
		LeaveWriter = PointDataFacade->GetWritable<FVector>(Settings->LeaveName, PCGExData::EBufferInit::Inherit);

		TArray<int32> SimplifiedIndices;
		SimplifiedIndices.Init(INDEX_NONE, Mask.Num());

		for (int i = 0; i < SimplifiedResult.Num(); i++)
		{
			const PCGExPaths::FSimplifiedPoint& Point = SimplifiedResult[i];
			SimplifiedIndices[Point.OriginalIndex] = i;
		}

		for (int i = 0; i < PointDataFacade->GetNum(); i++)
		{
			if (!PointFilterCache[i]) { continue; }

			const int j = SimplifiedIndices[i];
			if (j == INDEX_NONE)
			{
				Mask[i] = false;
				continue;
			}

			const PCGExPaths::FSimplifiedPoint& Point = SimplifiedResult[j];


			OutTransforms[i] = Point.Transform;
			ArriveWriter->SetValue(i, Point.TangentIn);
			LeaveWriter->SetValue(i, Point.TangentOut);
		}

		PointDataFacade->WriteFastest(TaskManager);

		return true;
	}

	void FProcessor::CompleteWork()
	{
		(void)PointDataFacade->Source->Gather(Mask);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
