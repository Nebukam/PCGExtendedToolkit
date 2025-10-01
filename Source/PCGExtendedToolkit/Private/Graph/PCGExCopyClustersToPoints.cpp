﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExCopyClustersToPoints.h"


#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Graph/Data/PCGExClusterData.h"


#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EIOInit UPCGExCopyClustersToPointsSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::NoInit; }
PCGExData::EIOInit UPCGExCopyClustersToPointsSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::NoInit; }

#pragma endregion

PCGEX_INITIALIZE_ELEMENT(CopyClustersToPoints)
PCGEX_ELEMENT_BATCH_EDGE_IMPL_ADV(CopyClustersToPoints)

TArray<FPCGPinProperties> UPCGExCopyClustersToPointsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGEx::SourceTargetsLabel, "Target points to copy clusters to.", Required)
	PCGExMatching::DeclareMatchingRulesInputs(DataMatching, PinProperties);
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExCopyClustersToPointsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGExMatching::DeclareMatchingRulesOutputs(DataMatching, PinProperties);
	return PinProperties;
}

bool FPCGExCopyClustersToPointsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(CopyClustersToPoints)

	Context->TargetsDataFacade = PCGExData::TryGetSingleFacade(Context, PCGEx::SourceTargetsLabel, false, true);
	if (!Context->TargetsDataFacade) { return false; }

	PCGEX_FWD(TransformDetails)
	if (!Context->TransformDetails.Init(Context, Context->TargetsDataFacade.ToSharedRef())) { return false; }

	PCGEX_FWD(TargetsAttributesToClusterTags)
	if (!Context->TargetsAttributesToClusterTags.Init(Context, Context->TargetsDataFacade)) { return false; }

	Context->MainDataMatcher = MakeShared<PCGExMatching::FDataMatcher>();
	Context->MainDataMatcher->SetDetails(&Settings->DataMatching);
	if (!Context->MainDataMatcher->Init(Context, {Context->TargetsDataFacade}, true)) { return false; }

	if (Settings->DataMatching.Mode != EPCGExMapMatchMode::Disabled &&
		Settings->DataMatching.ClusterMatchMode == EPCGExClusterComponentTagMatchMode::Separated)
	{
		Context->EdgeDataMatcher = MakeShared<PCGExMatching::FDataMatcher>();
		if (!Context->EdgeDataMatcher->Init(Context, Context->MainDataMatcher, PCGExMatching::SourceMatchRulesEdgesLabel, true)) { return false; }
	}
	else
	{
		Context->EdgeDataMatcher = Context->MainDataMatcher;
	}

	Context->TargetsForwardHandler = Settings->TargetsForwarding.GetHandler(Context->TargetsDataFacade);

	return true;
}

bool FPCGExCopyClustersToPointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCopyClustersToPointsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(CopyClustersToPoints)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExClusterMT::IBatch>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->OutputPointsAndEdges();
	Context->Done();

	return Context->TryComplete();
}

namespace PCGExCopyClustersToPoints
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		if (!IProcessor::Process(InAsyncManager)) { return false; }

		const int32 NumTargets = Context->TargetsDataFacade->GetNum();

		PCGEx::InitArray(EdgesDupes, NumTargets);

		MatchScope = PCGExMatching::FMatchingScope(GetParentBatch<FBatch>()->EdgesDataFacades->Num());
		InfiniteScope = PCGExMatching::FMatchingScope(Context->InitialMainPointsNum, true);

		StartParallelLoopForRange(NumTargets, 32);

		return true;
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		int32 Copies = 0;

		const bool bSameAnyChecks = Context->MainDataMatcher == Context->EdgeDataMatcher;

		PCGEX_SCOPE_LOOP(i)
		{
			EdgesDupes[i] = nullptr;

			if (!(*VtxDupes)[i]) { continue; }

			const PCGExData::FConstPoint TargetPoint = Context->TargetsDataFacade->GetInPoint(i);
			switch (Settings->DataMatching.ClusterMatchMode)
			{
			case EPCGExClusterComponentTagMatchMode::Vtx:
				// Handled by VtxDupe check
				break;
			case EPCGExClusterComponentTagMatchMode::Both:
			case EPCGExClusterComponentTagMatchMode::Edges:
			case EPCGExClusterComponentTagMatchMode::Separated:
				if (!Context->EdgeDataMatcher->Test(TargetPoint, EdgeDataFacade->Source, MatchScope)) { continue; }
				break;
			case EPCGExClusterComponentTagMatchMode::Any:
				if (bSameAnyChecks)
				{
					if (Context->EdgeDataMatcher->Test(TargetPoint, EdgeDataFacade->Source, MatchScope)) { continue; }
				}
				else
				{
					if (Context->MainDataMatcher->Test(TargetPoint, VtxDataFacade->Source, InfiniteScope) ||
						Context->EdgeDataMatcher->Test(TargetPoint, EdgeDataFacade->Source, MatchScope))
					{
						continue;
					}
				}
				break;
			}

			Copies++;

			// Create an edge copy per target point
			TSharedPtr<PCGExData::FPointIO> EdgeDupe = Context->MainEdges->Emplace_GetRef(EdgeDataFacade->Source, PCGExData::EIOInit::Duplicate);

			EdgesDupes[i] = EdgeDupe;
			PCGExGraph::MarkClusterEdges(EdgeDupe, *(VtxTag->GetData() + i));

			PCGEX_LAUNCH(PCGExGeoTasks::FTransformPointIO, i, Context->TargetsDataFacade->Source, EdgeDupe, &Context->TransformDetails)
		}

		if (Copies > 0) { FPlatformAtomics::InterlockedAdd(&NumCopies, Copies); }
	}

	void FProcessor::OnRangeProcessingComplete()
	{
		if (NumCopies == 0)
		{
			(void)Context->EdgeDataMatcher->HandleUnmatchedOutput(EdgeDataFacade, true);
		}
	}

	void FProcessor::CompleteWork()
	{
		if (NumCopies == 0) { return; }

		const UPCGBasePointData* InTargetsData = Context->TargetsDataFacade->GetIn();
		const int32 NumTargets = InTargetsData->GetNumPoints();

		// Once work is complete, check if there are cached clusters we can forward
		const TSharedPtr<PCGExCluster::FCluster> CachedCluster = PCGExClusterData::TryGetCachedCluster(VtxDataFacade->Source, EdgeDataFacade->Source);

		for (int i = 0; i < NumTargets; i++)
		{
			TSharedPtr<PCGExData::FPointIO> EdgeDupe = EdgesDupes[i];

			if (!EdgeDupe) { continue; }

			Context->TargetsAttributesToClusterTags.Tag(Context->TargetsDataFacade->GetInPoint(i), EdgeDupe);
			Context->TargetsForwardHandler->Forward(i, EdgeDupe->GetOut()->Metadata);
		}

		if (!CachedCluster) { return; }

		for (int i = 0; i < NumTargets; i++)
		{
			TSharedPtr<PCGExData::FPointIO> VtxDupe = *(VtxDupes->GetData() + i);
			TSharedPtr<PCGExData::FPointIO> EdgeDupe = EdgesDupes[i];

			if (!EdgeDupe) { continue; }

			UPCGExClusterEdgesData* EdgeDupeTypedData = Cast<UPCGExClusterEdgesData>(EdgeDupe->GetOut());
			if (CachedCluster && EdgeDupeTypedData)
			{
				EdgeDupeTypedData->SetBoundCluster(
					MakeShared<PCGExCluster::FCluster>(
						CachedCluster.ToSharedRef(), VtxDupe, EdgeDupe, CachedCluster->NodeIndexLookup,
						false, false, false));
			}
		}
	}

	FBatch::~FBatch()
	{
	}

	void FBatch::Process()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(CopyClustersToPoints)

		const int32 NumTargets = Context->TargetsDataFacade->GetNum();

		PCGEx::InitArray(VtxDupes, NumTargets);
		PCGEx::InitArray(VtxTag, NumTargets);

		PCGExMatching::FMatchingScope MatchScope = PCGExMatching::FMatchingScope(Context->InitialMainPointsNum);

		for (int i = 0; i < NumTargets; i++)
		{
			VtxDupes[i] = nullptr;
			VtxTag[i] = nullptr;

			const PCGExData::FConstPoint TargetPoint = Context->TargetsDataFacade->GetInPoint(i);

			switch (Settings->DataMatching.ClusterMatchMode)
			{
			case EPCGExClusterComponentTagMatchMode::Vtx:
			case EPCGExClusterComponentTagMatchMode::Both:
			case EPCGExClusterComponentTagMatchMode::Separated:
				if (!Context->MainDataMatcher->Test(TargetPoint, VtxDataFacade->Source, MatchScope)) { continue; }
				break;
			case EPCGExClusterComponentTagMatchMode::Edges:
			case EPCGExClusterComponentTagMatchMode::Any:
				// Ignore
				break;
			}

			NumCopies++;

			// Create a vtx copy per target point
			TSharedPtr<PCGExData::FPointIO> VtxDupe = Context->MainPoints->Emplace_GetRef(VtxDataFacade->Source, PCGExData::EIOInit::Duplicate);

			PCGExCommon::DataIDType OutId;
			PCGExGraph::SetClusterVtx(VtxDupe, OutId);

			VtxDupes[i] = VtxDupe;
			VtxTag[i] = OutId;

			PCGEX_LAUNCH(PCGExGeoTasks::FTransformPointIO, i, Context->TargetsDataFacade->Source, VtxDupe, &Context->TransformDetails)

			Context->TargetsAttributesToClusterTags.Tag(Context->TargetsDataFacade->GetInPoint(i), VtxDupe);
			Context->TargetsForwardHandler->Forward(i, VtxDupe->GetOut()->Metadata);
		}

		TBatch<FProcessor>::Process();
	}

	bool FBatch::PrepareSingle(const TSharedPtr<PCGExClusterMT::IProcessor>& InProcessor)
	{
		if (!TBatch<FProcessor>::PrepareSingle(InProcessor)) { return false; }
		PCGEX_TYPED_PROCESSOR
		TypedProcessor->VtxDupes = &VtxDupes;
		TypedProcessor->VtxTag = &VtxTag;
		return true;
	}

	void FBatch::CompleteWork()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(CopyClustersToPoints)

		for (int Pi = 0; Pi < Processors.Num(); Pi++)
		{
			if (GetProcessor<FProcessor>(Pi)->NumCopies == 0)
			{
				(void)Context->EdgeDataMatcher->HandleUnmatchedOutput(VtxDataFacade, true);
				break;
			}
		}

		const int32 NumTargets = Context->TargetsDataFacade->GetIn()->GetNumPoints();

		for (int i = 0; i < NumTargets; i++)
		{
			if (!VtxDupes[i]) { continue; }

			bool bValidVtxDupe = false;
			for (const TSharedRef<PCGExClusterMT::IProcessor>& InProcessor : Processors)
			{
				PCGEX_TYPED_PROCESSOR_REF
				if (TypedProcessor->EdgesDupes[i])
				{
					bValidVtxDupe = true;
					break;
				}
			}

			if (!bValidVtxDupe)
			{
				VtxDupes[i]->InitializeOutput(PCGExData::EIOInit::NoInit);
				VtxDupes[i]->Disable();
				VtxDupes[i] = nullptr;
			}
		}

		TBatch<FProcessor>::CompleteWork();
	}
}
#undef LOCTEXT_NAMESPACE
