// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExChamferPath.h"

#include "Data/PCGExPointFilter.h"

#define LOCTEXT_NAMESPACE "PCGExChamferPathElement"
#define PCGEX_NAMESPACE ChamferPath

TArray<FPCGPinProperties> UPCGExChamferPathSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExChamferPath::SourceChamferFilters, "Filters used to know if a point should be chamfered", Advanced, {})
	return PinProperties;
}

PCGExData::EInit UPCGExChamferPathSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(ChamferPath)

FPCGExChamferPathContext::~FPCGExChamferPathContext()
{
	PCGEX_TERMINATE_ASYNC

	ChamferFilterFactories.Empty();

	PCGEX_DELETE(MainPaths)
}

bool FPCGExChamferPathElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ChamferPath)

	GetInputFactories(Context, PCGExChamferPath::SourceChamferFilters, Context->ChamferFilterFactories, PCGExFactories::PointFilters, false);

	Context->MainPaths = new PCGExData::FPointIOCollection(Context);
	Context->MainPaths->DefaultOutputLabel = Settings->GetMainOutputLabel();

	return true;
}

bool FPCGExChamferPathElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExChamferPathElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(ChamferPath)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		bool bHasInvalildInputs = false;
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExChamferPath::FProcessor>>(
			[&](PCGExData::FPointIO* Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bHasInvalildInputs = true;
					return false;
				}

				return true;
			},
			[&](PCGExPointsMT::TBatch<PCGExChamferPath::FProcessor>* NewBatch)
			{
				NewBatch->SetPointsFilterData(&Context->FilterFactories);
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not find any paths to Chamfer."));
			return true;
		}

		if (bHasInvalildInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 2 points and won't be processed."));
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->MainPaths->Pairs.Reserve(Context->MainPaths->Pairs.Num() + Context->MainBatch->GetNumProcessors());
	Context->MainBatch->Output();

	Context->MainPaths->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExChamferPath
{
	FProcessor::~FProcessor()
	{
		
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExChamferPath::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ChamferPath)

		LocalTypedContext = TypedContext;
		LocalSettings = Settings;

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		// TODO : Init new output or forward based on filters Point->InitializeOutput(PCGExData::EInit::NewOutput);
		
		bInlineProcessPoints = true;
		bClosedPath = Settings->bClosedPath;

		PCGEX_SET_NUM_UNINITIALIZED(DoChamfer, PointIO->GetNum())

		if (!TypedContext->ChamferFilterFactories.IsEmpty())
		{
			PCGExPointFilter::TManager* FilterManager = new PCGExPointFilter::TManager(PointDataFacade);
			if (!FilterManager->Init(Context, TypedContext->ChamferFilterFactories))
			{
				PCGEX_DELETE(FilterManager)
				return false;
			}

			for (int i = 0; i < DoChamfer.Num(); i++) { DoChamfer[i] = FilterManager->Test(i); }
			PCGEX_DELETE(FilterManager)
		}
		else { for (bool& Chamfer : DoChamfer) { Chamfer = true; } }

		StartParallelLoopForPoints(PCGExData::ESource::In);

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount)
	{
		bool bChamfer = DoChamfer[Index];
		
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount)
	{
		
	}

	void FProcessor::CompleteWork()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ChamferPath)

		
	}
	
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
