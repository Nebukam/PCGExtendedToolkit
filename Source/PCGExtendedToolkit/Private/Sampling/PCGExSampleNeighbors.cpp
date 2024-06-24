// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNeighbors.h"

#include "Sampling/Neighbors/PCGExNeighborSampleAttribute.h"
#include "Sampling/Neighbors/PCGExNeighborSampleFactoryProvider.h"

#define LOCTEXT_NAMESPACE "PCGExSampleNeighbors"
#define PCGEX_NAMESPACE SampleNeighbors

UPCGExSampleNeighborsSettings::UPCGExSampleNeighborsSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TArray<FPCGPinProperties> UPCGExSampleNeighborsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExNeighborSample::SourceSamplersLabel, "Neighbor samplers.", Required, {})
	return PinProperties;
}

PCGExData::EInit UPCGExSampleNeighborsSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::Forward; }
PCGExData::EInit UPCGExSampleNeighborsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(SampleNeighbors)

FPCGExSampleNeighborsContext::~FPCGExSampleNeighborsContext()
{
	PCGEX_TERMINATE_ASYNC

	//for (UPCGExNeighborSamplerFactoryBase* Factory : SamplerFactories) { PCGEX_DELETE_UOBJECT(Factory) }
	SamplerFactories.Empty();
}

bool FPCGExSampleNeighborsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleNeighbors)

	if (!PCGExFactories::GetInputFactories(InContext, PCGExNeighborSample::SourceSamplersLabel, Context->SamplerFactories, {PCGExFactories::EType::Sampler}, false))
	{
		PCGE_LOG(Warning, GraphAndLog, FTEXT("No valid sampler found."));
		return false;
	}

	// Sort samplers so higher priorities come last, as they have to potential to override values.
	Context->SamplerFactories.Sort([&](const UPCGExNeighborSamplerFactoryBase& A, const UPCGExNeighborSamplerFactoryBase& B) { return A.Priority < B.Priority; });

	return true;
}

bool FPCGExSampleNeighborsElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleNeighborsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SampleNeighbors)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartProcessingClusters<PCGExClusterMT::TBatch<PCGExSampleNeighbors::FProcessor>>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExClusterMT::TBatch<PCGExSampleNeighbors::FProcessor>* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}
	}

	if (!Context->ProcessClusters()) { return false; }


	if (Context->IsDone())
	{
		Context->OutputPointsAndEdges();
	}

	return Context->TryComplete();
}


namespace PCGExSampleNeighbors
{
	FProcessor::~FProcessor()
	{
		for (UPCGExNeighborSampleOperation* Op : SamplingOperations)
		{
			if (Op->PointState) { PCGEX_DELETE(Op->PointState) }
			if (Op->ValueState) { PCGEX_DELETE(Op->ValueState) }
			PCGEX_DELETE_OPERATION(Op)
		}
		SamplingOperations.Empty();

		VtxOps.Empty();
		EdgeOps.Empty();
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SampleNeighbors)

		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		TArray<UPCGExNeighborSampleOperation*> PointStatePreparation;
		TArray<UPCGExNeighborSampleOperation*> ValueStatePreparation;

		for (const UPCGExNeighborSamplerFactoryBase* OperationFactory : TypedContext->SamplerFactories)
		{
			UPCGExNeighborSampleOperation* SamplingOperation = OperationFactory->CreateOperation();
			SamplingOperation->BindContext(TypedContext);
			SamplingOperation->PrepareForCluster(Context, Cluster);

			if (!SamplingOperation->IsOperationValid())
			{
				PCGEX_DELETE_OPERATION(SamplingOperation)
				continue;
			}

			SamplingOperations.Add(SamplingOperation);

			if (SamplingOperation->PointState && SamplingOperation->PointState->RequiresPerPointPreparation()) { PointStatePreparation.Add(SamplingOperation); }
			if (SamplingOperation->ValueState && SamplingOperation->ValueState->RequiresPerPointPreparation()) { ValueStatePreparation.Add(SamplingOperation); }

			if (SamplingOperation->BaseSettings.NeighborSource == EPCGExGraphValueSource::Point) { VtxOps.Add(SamplingOperation); }
			else { EdgeOps.Add(SamplingOperation); }
		}

		for (int i = 0; i < Cluster->Nodes->Num(); i++)
		{
			for (const UPCGExNeighborSampleOperation* Op : PointStatePreparation) { Op->PointState->PrepareSingle(i); }
			for (const UPCGExNeighborSampleOperation* Op : ValueStatePreparation) { Op->ValueState->PrepareSingle(i); }
		}

		StartParallelLoopForNodes();

		return true;
	}

	void FProcessor::ProcessSingleNode(PCGExCluster::FNode& Node)
	{
		for (const UPCGExNeighborSampleOperation* Op : VtxOps) { Op->ProcessNodeForPoints(Node.NodeIndex); }
		for (const UPCGExNeighborSampleOperation* Op : EdgeOps) { Op->ProcessNodeForEdges(Node.NodeIndex); }
	}

	void FProcessor::CompleteWork()
	{
		for (UPCGExNeighborSampleOperation* Op : SamplingOperations) { Op->FinalizeOperation(); }
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
