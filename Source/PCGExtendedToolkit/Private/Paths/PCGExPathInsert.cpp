﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathInsert.h"

#include "Data/PCGExPointIO.h"
#include "Details/PCGExDetailsDistances.h"
#include "Details/PCGExDetailsSettings.h"

#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExPathInsertElement"
#define PCGEX_NAMESPACE PathInsert

PCGEX_SETTING_VALUE_IMPL(UPCGExPathInsertSettings, Range, int32, RangeInput, RangeAttribute, Range)

TArray<FPCGPinProperties> UPCGExPathInsertSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINTS(PCGEx::SourceTargetsLabel, "The point data set to insert.", Required)
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(PathInsert)
PCGEX_ELEMENT_BATCH_POINT_IMPL(PathInsert)

bool FPCGExPathInsertElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathInsert)

	Context->Distances = PCGExDetails::MakeDistances();

	return true;
}

bool FPCGExPathInsertElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathInsertElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathInsert)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 2 points and won't be processed."))

		const bool bIsCanBeCutTagValid = PCGEx::IsValidStringTag(Context->CanBeCutTag);

		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 2)
				{
					Entry->InitializeOutput(PCGExData::EIOInit::Forward);
					bHasInvalidInputs = true;
					return false;
				}
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
				//NewBatch->SetPointsFilterData(&Context->FilterFactories);
				//NewBatch->bRequiresWriteStep = Settings->bDoCrossBlending;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any paths to intersect with."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	PCGEX_OUTPUT_VALID_PATHS(MainPoints)

	return Context->TryComplete();
}

namespace PCGExPathInsert
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPathInsert::Process);

		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;

		// Must be set before process for filters
		//PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		bClosedLoop = PCGExPaths::GetClosedLoop(PointIO->GetIn());

		Path = MakeShared<PCGExPaths::FPath>(PointIO->GetIn(), 0); //Details.Tolerance * 2);
		Path->IOIndex = PointDataFacade->Source->IOIndex;
		PathLength = Path->AddExtra<PCGExPaths::FPathEdgeLength>();

		Path->IOIndex = PointIO->IOIndex;

		PCGExMT::FScope EdgesScope = Path->GetEdgeScope();

		Path->ComputeAllEdgeExtra();

		return true;
	}

	void FProcessor::CompleteWork()
	{
		StartParallelLoopForRange(Path->NumEdges);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
