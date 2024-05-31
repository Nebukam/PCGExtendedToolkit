// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNeighbors.h"

#include "Graph/Pathfinding/PCGExPathfinding.h"
#include "Sampling/Neighbors/PCGExNeighborSampleFactoryProvider.h"
#include "Sampling/Neighbors/PCGExNeighborSampleOperation.h"

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
	PCGEX_PIN_PARAMS(PCGExNeighborSample::SourceSamplersLabel, "Neighbor samplers.", false, {})
	return PinProperties;
}

PCGExData::EInit UPCGExSampleNeighborsSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::Forward; }
PCGExData::EInit UPCGExSampleNeighborsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

bool FPCGExSampleNeighborsContext::PrepareSettings(
	FPCGExBlendingSettings& OutSettings,
	TArray<UPCGExNeighborSampleOperation*>& OutOperations,
	const PCGExData::FPointIO& FromPointIO,
	EPCGExGraphValueSource Source) const
{
	OutSettings = FPCGExBlendingSettings(EPCGExDataBlendingType::None);
	OutSettings.BlendingFilter = EPCGExBlendingFilter::Include;

	OutOperations.Empty();

	TArray<PCGEx::FAttributeIdentity> OutIdentities;
	PCGEx::FAttributeIdentity::Get(FromPointIO.GetIn()->Metadata, OutIdentities);

	for (UPCGExNeighborSampleOperation* Operation : SamplingOperations)
	{
		if (Operation->NeighborSource != Source) { continue; }

		bool bAttributeExists = false;
		for (PCGEx::FAttributeIdentity Identity : OutIdentities)
		{
			if (Identity.Name == Operation->SourceAttribute)
			{
				bAttributeExists = true;
				break;
			}
		}

		if (!bAttributeExists)
		{
			PCGE_LOG_C(Warning, GraphAndLog, this, FText::Format(FTEXT("Missing source attribute: {0}."), FText::FromName(Operation->SourceAttribute)));
			continue;
		}

		OutSettings.AttributesOverrides.Add(Operation->SourceAttribute, Operation->Blending);
		OutSettings.FilteredAttributes.Add(Operation->SourceAttribute);
		OutOperations.Add(Operation);
	}

	return !OutSettings.FilteredAttributes.IsEmpty();
}

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

	PCGEX_DELETE(BlenderFromPoints)
	PCGEX_DELETE(BlenderFromEdges)
}

bool FPCGExSampleNeighborsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleNeighbors)

	const TArray<FPCGTaggedData>& Inputs = InContext->InputData.GetInputsByPin(PCGExNeighborSample::SourceSamplersLabel);

	for (const FPCGTaggedData& InputState : Inputs)
	{
		if (const UPCGNeighborSamplerFactoryBase* OperationFactory = Cast<UPCGNeighborSamplerFactoryBase>(InputState.Data))
		{
			if (!PCGEx::IsValidName(OperationFactory->Descriptor.SourceAttribute))
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("A source sampler name is invalid and will be ignored."));
				continue;
			}

			if (OperationFactory->Descriptor.bOutputToNewAttribute &&
				!PCGEx::IsValidName(OperationFactory->Descriptor.TargetAttribute))
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("A target sampler name is invalid and will be ignored."));
				continue;
			}

			UPCGExNeighborSampleOperation* Operation = OperationFactory->CreateOperation();
			Context->SamplingOperations.Add(Operation);
			Context->RegisterOperation<UPCGExNeighborSampleOperation>(Operation);
		}
	}

	if (Context->SamplingOperations.IsEmpty())
	{
		PCGE_LOG(Warning, GraphAndLog, FTEXT("No valid sampler found."));
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
		Context->PointPointBlendingSettings = FPCGExBlendingSettings(EPCGExDataBlendingType::None);

		PCGEX_DELETE(Context->BlenderFromPoints)

		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (!Context->TaggedEdges)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input points have no associated edges."));
				Context->SetState(PCGExMT::State_ReadyForNextPoints);
				return false;
			}

			if (Context->PrepareSettings(
				Context->PointPointBlendingSettings,
				Context->PointPointOperations,
				*Context->CurrentIO,
				EPCGExGraphValueSource::Point))
			{
				Context->BlenderFromPoints = new PCGExDataBlending::FMetadataBlender(&Context->PointPointBlendingSettings);
				Context->BlenderFromPoints->PrepareForData(*Context->CurrentIO);
			}

			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		Context->PointEdgeBlendingSettings = FPCGExBlendingSettings(EPCGExDataBlendingType::None);

		PCGEX_DELETE(Context->BlenderFromEdges)

		if (!Context->AdvanceEdges(true))
		{
			if (Context->BlenderFromPoints) { Context->BlenderFromPoints->Write(); }

			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			return false;
		}

		if (!Context->CurrentCluster) { return false; } // Corrupted or invalid cluster

		if (Context->PrepareSettings(
			Context->PointEdgeBlendingSettings,
			Context->PointEdgeOperations,
			*Context->CurrentEdges,
			EPCGExGraphValueSource::Edge))
		{
			Context->BlenderFromEdges = new PCGExDataBlending::FMetadataBlender(&Context->PointEdgeBlendingSettings);
			Context->BlenderFromEdges->PrepareForData(*Context->CurrentIO, *Context->CurrentEdges);
		}

		if (!Context->BlenderFromPoints && !Context->BlenderFromEdges) { return false; } // Nothing to blend

		Context->SetState(PCGExData::State_MergingData);
	}

#define PCGEX_NODE_TARGET \
	const PCGExCluster::FNode& Node = Context->CurrentCluster->Nodes[NodeIndex]; \
	PCGEx::FPointRef Target = Context->CurrentIO->GetOutPointRef(Node.PointIndex);

	if (Context->IsState(PCGExData::State_MergingData))
	{
		auto Initialize = [&]()
		{
			for (UPCGExNeighborSampleOperation* Operation : Context->PointPointOperations)
			{
				Operation->Blender = Context->BlenderFromPoints;
				Operation->PrepareForCluster(Context->CurrentCluster);
			}

			for (UPCGExNeighborSampleOperation* Operation : Context->PointEdgeOperations)
			{
				Operation->Blender = Context->BlenderFromEdges;
				Operation->PrepareForCluster(Context->CurrentCluster);
			}
		};

		auto ProcessNode = [&](const int32 NodeIndex)
		{
			PCGEX_NODE_TARGET
			for (const UPCGExNeighborSampleOperation* Operation : Context->PointPointOperations) { Operation->ProcessNodeForPoints(Target, Node); }
			for (const UPCGExNeighborSampleOperation* Operation : Context->PointEdgeOperations) { Operation->ProcessNodeForEdges(Target, Node); }
		};

		/*
		Initialize();
		
		for(int i = 0; i < Context->CurrentCluster->Nodes.Num(); i++)		{			Context->GetAsyncManager()->Start<FPCGExSampleNeighborTask>(i, Context->CurrentIO);		}

		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
		*/
		if (!Context->Process(Initialize, ProcessNode, Context->CurrentCluster->Nodes.Num())) { return false; }

		if (Context->BlenderFromEdges) { Context->BlenderFromEdges->Write(); }

		Context->SetState(PCGExGraph::State_ReadyForNextEdges);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_WAIT_ASYNC

		if (Context->BlenderFromEdges) { Context->BlenderFromEdges->Write(); }

		Context->SetState(PCGExGraph::State_ReadyForNextEdges);
	}

	if (Context->IsDone())
	{
		Context->OutputPointsAndEdges();
	}

	return Context->IsDone();
}

bool FPCGExSampleNeighborTask::ExecuteTask()
{
	const FPCGExSampleNeighborsContext* Context = Manager->GetContext<FPCGExSampleNeighborsContext>();

	const PCGExCluster::FNode& Node = Context->CurrentCluster->Nodes[TaskIndex];
	const PCGEx::FPointRef Target = Context->CurrentIO->GetOutPointRef(Node.PointIndex);

	for (const UPCGExNeighborSampleOperation* Operation : Context->PointPointOperations) { Operation->ProcessNodeForPoints(Target, Node); }
	for (const UPCGExNeighborSampleOperation* Operation : Context->PointEdgeOperations) { Operation->ProcessNodeForEdges(Target, Node); }

	return true;
}

#undef PCGEX_NODE_TARGET

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
