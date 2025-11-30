// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExWriteStates.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Graph/PCGExCluster.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE WriteStates

PCGExData::EIOInit UPCGExWriteStatesSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

TArray<FPCGPinProperties> UPCGExWriteStatesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_FACTORIES(PCGExPointStates::SourceStatesLabel, "Point states.", Required, FPCGExDataTypeInfoPointState::AsId())
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(WriteStates)
PCGEX_ELEMENT_BATCH_POINT_IMPL(WriteStates)

bool FPCGExWriteStatesElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WriteStates)

	return PCGExFactories::GetInputFactories(
		Context, PCGExPointStates::SourceStatesLabel, Context->StateFactories,
		{PCGExFactories::EType::PointState});
}

bool FPCGExWriteStatesElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExWriteStatesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(WriteStates)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

UPCGExFactoryData* UPCGExPointStateFactoryProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGExPointStateFactoryData* NewFactory = InContext->ManagedObjects->New<UPCGExPointStateFactoryData>();
	NewFactory->BaseConfig = Config;
	Super::CreateFactory(InContext, NewFactory);
	return NewFactory;
}

namespace PCGExWriteStates
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFindNodeState::Process);
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		const TSharedPtr<PCGExData::TBuffer<int64>> Writer = PointDataFacade->GetWritable(Settings->FlagAttribute, Settings->InitialFlags, false, PCGExData::EBufferInit::Inherit);
		const TSharedPtr<PCGExData::TArrayBuffer<int64>> ElementsWriter = StaticCastSharedPtr<PCGExData::TArrayBuffer<int64>>(Writer);
		StateManager = MakeShared<PCGExPointStates::FStateManager>(ElementsWriter->GetOutValues(), PointDataFacade);
		StateManager->Init(ExecutionContext, Context->StateFactories);

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		PointDataFacade->Fetch(Scope);
		PCGEX_SCOPE_LOOP(Index) { StateManager->Test(Index); }
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->WriteFastest(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
