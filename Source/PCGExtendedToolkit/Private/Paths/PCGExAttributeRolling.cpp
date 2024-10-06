// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExAttributeRolling.h"

#include "Data/PCGExPointFilter.h"
#include "Data/Blending/PCGExMetadataBlender.h"


#define LOCTEXT_NAMESPACE "PCGExAttributeRollingElement"
#define PCGEX_NAMESPACE AttributeRolling

UPCGExAttributeRollingSettings::UPCGExAttributeRollingSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportClosedLoops = false;
}

TArray<FPCGPinProperties> UPCGExAttributeRollingSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExPaths::SourceTriggerFilters, "Filters used to check if a point triggers the select behavior.", Normal, {})
	return PinProperties;
}

PCGExData::EInit UPCGExAttributeRollingSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(AttributeRolling)

bool FPCGExAttributeRollingElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(AttributeRolling)

	//PCGEX_FWD(bDoBlend)
	//PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendInterpolate)

	return true;
}


bool FPCGExAttributeRollingElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExAttributeRollingElement::Execute);

	PCGEX_CONTEXT(AttributeRolling)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		bool bInvalidInputs = false;

		// TODO : Skip completion

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExAttributeRolling::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExAttributeRolling::FProcessor>>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any paths to fuse."));
		}

		if (bInvalidInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 2 points and won't be processed."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainBatch->Output();
	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExAttributeRolling
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExAttributeRolling::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }


		MetadataBlender = MakeUnique<PCGExDataBlending::FMetadataBlender>(&Settings->BlendingSettings);
		MetadataBlender->PrepareForData(PointDataFacade, PCGExData::ESource::In, true, nullptr, true);

		OutMetadata = PointDataFacade->GetOut()->Metadata;
		OutPoints = &PointDataFacade->GetOut()->GetMutablePoints();

		bInlineProcessPoints = true;
		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount)
	{
		OutMetadata->InitializeOnSet(Point.MetadataEntry);

		if (Index == 0) { return; }

		bool bSkipPoint = false; // Filter driven

		const FPCGPoint& PrevPoint = *(OutPoints->GetData() + (Index - 1));
		const FPCGPoint& InPoint = PointDataFacade->Source->GetInPoint(Index);

		MetadataBlender->PrepareForBlending(Point);
		MetadataBlender->Blend(PrevPoint, InPoint, Point, 1);
		MetadataBlender->CompleteBlending(Point, 2, 1);
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
