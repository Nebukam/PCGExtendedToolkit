// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNeighbors.h"

#include "Graph/Pathfinding/PCGExPathfinding.h"
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

	for (UPCGExNeighborSampleOperation* Operation : SamplingOperations)
	{
		Operation->Cleanup();
		PCGEX_DELETE_UOBJECT(Operation)
	}

	SamplingOperations.Empty();
}

bool FPCGExSampleNeighborsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleNeighbors)

	TArray<UPCGNeighborSamplerFactoryBase*> SamplerFactories;

	if (!PCGExFactories::GetInputFactories(InContext, PCGExNeighborSample::SourceSamplersLabel, SamplerFactories, {PCGExFactories::EType::Sampler}, false))
	{
		PCGE_LOG(Warning, GraphAndLog, FTEXT("No valid sampler found."));
		return false;
	}

	// Sort samplers so higher priorities come last, as they have to potential to override values.
	SamplerFactories.Sort([&](const UPCGNeighborSamplerFactoryBase& A, const UPCGNeighborSamplerFactoryBase& B) { return A.Priority < B.Priority; });

	for (const UPCGNeighborSamplerFactoryBase* OperationFactory : SamplerFactories)
	{
		UPCGExNeighborSampleOperation* Operation = OperationFactory->CreateOperation();
		Context->SamplingOperations.Add(Operation);
		Context->RegisterOperation<UPCGExNeighborSampleOperation>(Operation);
	}

	if (Context->SamplingOperations.IsEmpty())
	{
		PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not find any valid samplers."));
		return false;
	}

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
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (!Context->TaggedEdges)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input points have no associated edges."));
				Context->SetState(PCGExMT::State_ReadyForNextPoints);
				return false;
			}

			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		if (!Context->AdvanceEdges(true))
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			return false;
		}

		if (!Context->CurrentCluster) { return false; } // Corrupted or invalid cluster

		Context->SetState(PCGExSampleNeighbors::State_ReadyForNextOperation);
	}

	if (Context->IsState(PCGExSampleNeighbors::State_ReadyForNextOperation))
	{
		const int32 NextIndex = Context->CurrentOperation ? Context->SamplingOperations.Find(Context->CurrentOperation) + 1 : 0;

		if (Context->SamplingOperations.IsValidIndex(NextIndex))
		{
			Context->CurrentOperation = Context->SamplingOperations[NextIndex];
			Context->CurrentOperation->PrepareForCluster(Context, Context->CurrentCluster);

			if (!Context->CurrentOperation->IsOperationValid()) { return false; }

			Context->SetState(PCGExSampleNeighbors::State_Sampling);
		}
		else
		{
			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
		}
	}

	if (Context->IsState(PCGExSampleNeighbors::State_Sampling))
	{
		auto ProcessNodePoints = [&](const int32 NodeIndex) { Context->CurrentOperation->ProcessNodeForPoints(NodeIndex); };
		auto ProcessNodeEdges = [&](const int32 NodeIndex) { Context->CurrentOperation->ProcessNodeForEdges(NodeIndex); };

		if (Context->CurrentOperation->BaseSettings.NeighborSource == EPCGExGraphValueSource::Point)
		{
			if (!Context->Process(ProcessNodePoints, Context->CurrentCluster->Nodes.Num())) { return false; }
		}
		else
		{
			if (!Context->Process(ProcessNodeEdges, Context->CurrentCluster->Nodes.Num())) { return false; }
		}

		Context->CurrentOperation->FinalizeOperation();
		Context->SetState(PCGExSampleNeighbors::State_ReadyForNextOperation);
	}

	if (Context->IsDone())
	{
		Context->OutputPointsAndEdges();
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
