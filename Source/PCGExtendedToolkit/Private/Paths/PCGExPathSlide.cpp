﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathSlide.h"

#include "Data/PCGExPointIO.h"
#include "Details/PCGExDetailsSettings.h"
#include "Paths/PCGExPaths.h"

#define LOCTEXT_NAMESPACE "PCGExPathSlideElement"
#define PCGEX_NAMESPACE PathSlide

PCGEX_INITIALIZE_ELEMENT(PathSlide)
PCGEX_ELEMENT_BATCH_POINT_IMPL(PathSlide)

PCGEX_SETTING_VALUE_IMPL(UPCGExPathSlideSettings, SlideAmount, double, SlideAmountInput, SlideAmountAttribute, SlideAmountConstant)

bool FPCGExPathSlideElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathSlide)

	if (Settings->Mode == EPCGExSlideMode::Restore || Settings->bWriteOldPosition)
	{
		PCGEX_VALIDATE_NAME(Settings->RestorePositionAttributeName);
	}

	return true;
}

bool FPCGExPathSlideElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathSlideElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathSlide)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some input have less than 2 points and will be ignored."))
		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				PCGEX_SKIP_INVALID_PATH_ENTRY
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any valid path."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExPathSlide
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPathSlide::Process);

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;
		bClosedLoop = PCGExPaths::GetClosedLoop(PointIO->GetIn());

		PointDataFacade->GetOut()->AllocateProperties(EPCGPointNativeProperties::Transform);

		if (Settings->Mode == EPCGExSlideMode::Slide)
		{
			Path = MakeShared<PCGExPaths::FPath>(PointDataFacade->GetIn(), 0);
			Path->IOIndex = PointDataFacade->Source->IOIndex;

			SlideAmountGetter = Settings->GetValueSettingSlideAmount();
			if (!SlideAmountGetter->Init(PointDataFacade, false)) { return false; }

			if (Settings->bWriteOldPosition)
			{
				RestorePositionBuffer = PointDataFacade->GetWritable<FVector>(Settings->RestorePositionAttributeName, FVector::ZeroVector, true, PCGExData::EBufferInit::New);
				if (!RestorePositionBuffer) { return false; }
			}

			StartParallelLoopForPoints();
		}
		else
		{
			RestorePositionBuffer = PointDataFacade->GetBroadcaster<FVector>(Settings->RestorePositionAttributeName, true);
			if (!RestorePositionBuffer) { return false; }

			StartParallelLoopForRange(PointDataFacade->GetNum());
		}


		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::PathSlide::ProcessPoints);

		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);

		TPCGValueRange<FTransform> OutTransforms = PointDataFacade->GetOut()->GetTransformValueRange(false);
		const int32 Offset = Settings->Direction == EPCGExSlideDirection::Next ? 1 : -1;

		PCGEX_SCOPE_LOOP(Index)
		{
			const FVector From = Path->GetPos(Index);
			if (RestorePositionBuffer) { RestorePositionBuffer->SetValue(Index, From); }

			if (!PointFilterCache[Index]) { continue; }

			FVector To = Path->GetPos(Index + Offset);

			if (!bClosedLoop)
			{
				if (Offset < 0 && Index == 0)
				{
					To = Path->GetPos(Index + 1);
					To = From - (To - From);
				}
				else if (Offset > 0 && Index == Path->LastIndex)
				{
					To = Path->GetPos(Index - 1);
					To = From - (To - From);
				}
			}

			if (Settings->AmountMeasure == EPCGExMeanMeasure::Relative)
			{
				OutTransforms[Index].SetLocation(FMath::Lerp(From, To, SlideAmountGetter->Read(Index)));
			}
			else
			{
				OutTransforms[Index].SetLocation(From + ((To - From).GetSafeNormal() * SlideAmountGetter->Read(Index)));
			}
		}
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		if (RestorePositionBuffer) { PointDataFacade->WriteFastest(AsyncManager); }
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::PathSlide::ProcessPoints);

		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);

		TPCGValueRange<FTransform> OutTransforms = PointDataFacade->GetOut()->GetTransformValueRange(false);

		PCGEX_SCOPE_LOOP(Index)
		{
			if (!PointFilterCache[Index]) { continue; }
			OutTransforms[Index].SetLocation(RestorePositionBuffer->Read(Index));
		}
	}

	void FProcessor::OnRangeProcessingComplete()
	{
		PointDataFacade->Source->DeleteAttribute(RestorePositionBuffer->GetTypedInAttribute());
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
