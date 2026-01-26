// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/FloodFill/PCGExFloodFill.h"

#include "PCGExHeuristicsHandler.h"
#include "Clusters/PCGExCluster.h"
#include "Containers/PCGExHashLookup.h"
#include "Core/PCGExBlendOpsManager.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Data/Utils/PCGExDataForwardDetails.h"
#include "Paths/PCGExPath.h"

#include "Elements/FloodFill/FillControls/PCGExFillControlOperation.h"
#include "Elements/FloodFill/FillControls/PCGExFillControlsFactoryProvider.h"
#include "Paths/PCGExPathsCommon.h"

#define LOCTEXT_NAMESPACE "PCGExFloodFill"
#define PCGEX_NAMESPACE FloodFill

namespace PCGExFloodFill
{
	FDiffusion::FDiffusion(const TSharedPtr<FFillControlsHandler>& InFillControlsHandler, const TSharedPtr<PCGExClusters::FCluster>& InCluster, const PCGExClusters::FNode* InSeedNode)
		: FillControlsHandler(InFillControlsHandler), SeedNode(InSeedNode), Cluster(InCluster)
	{
		TravelStack = MakeShared<PCGEx::FHashLookupMap>(0, 0);
	}

	int32 FDiffusion::GetSettingsIndex(EPCGExFloodFillSettingSource Source) const
	{
		return Source == EPCGExFloodFillSettingSource::Seed ? SeedIndex : SeedNode->PointIndex;
	}

	void FDiffusion::Init(const int32 InSeedIndex)
	{
		SeedIndex = InSeedIndex;

		Visited.Add(SeedNode->Index);
		*(FillControlsHandler->InfluencesCount->GetData() + SeedNode->PointIndex) = 1;
		FCandidate& SeedCandidate = Captured.Emplace_GetRef();
		SeedCandidate.Link = PCGExGraphs::FLink(-1, -1);
		SeedCandidate.Node = SeedNode;
		SeedCandidate.CaptureIndex = 0;

		Probe(SeedCandidate);
	}

	void FDiffusion::Probe(const FCandidate& From)
	{
		if (!FillControlsHandler->IsValidProbe(this, From))
		{
			// Invalid as probe
			return;
		}

		// Gather all neighbors, add to candidates for the first time only
		bool bIsAlreadyInSet = false;

		const PCGExClusters::FNode& FromNode = *From.Node;
		FVector FromPosition = Cluster->GetPos(FromNode);

		for (const PCGExGraphs::FLink& Lk : FromNode.Links)
		{
			PCGExClusters::FNode* OtherNode = Cluster->GetNode(Lk);
			Visited.Add(OtherNode->Index, &bIsAlreadyInSet);

			if (bIsAlreadyInSet) { continue; }

			FVector OtherPosition = Cluster->GetPos(OtherNode);
			double Dist = FVector::Dist(FromPosition, OtherPosition);

			FCandidate Candidate = FCandidate{};
			Candidate.CaptureIndex = From.CaptureIndex;

			Candidate.Link = PCGExGraphs::FLink(FromNode.Index, Lk.Edge);
			Candidate.Node = OtherNode;

			Candidate.Depth = From.Depth + 1;
			Candidate.Distance = Dist;
			Candidate.PathDistance = From.PathDistance + Dist;

			// Scoring via fill controls (use 'Heuristics Score' fill control for heuristics-based scoring)
			FillControlsHandler->ScoreCandidate(this, From, Candidate);

			if (FillControlsHandler->IsValidCandidate(this, From, Candidate))
			{
				Candidates.Add(Candidate);
			}
		}
	}

	void FDiffusion::Grow()
	{
		if (bStopped) { return; }

		bool bSearch = true;
		while (bSearch)
		{
			if (Candidates.IsEmpty())
			{
				bStopped = true;
				break;
			}

			FCandidate Candidate = Candidates.Pop(EAllowShrinking::No);

			if (!FillControlsHandler->TryCapture(this, Candidate)) { continue; }

			// Update max depth & max distance
			MaxDepth = FMath::Max(MaxDepth, Candidate.Depth);
			MaxDistance = FMath::Max(MaxDistance, Candidate.PathDistance);

			FCandidate& CapturedCandidate = Captured.Add_GetRef(Candidate);
			CapturedCandidate.CaptureIndex = Captured.Num() - 1;

			TravelStack->Set(Candidate.Node->Index, PCGEx::NH64(Candidate.Link.Node, Candidate.Link.Edge));

			Endpoints.Add(CapturedCandidate.CaptureIndex);
			Endpoints.Remove(Candidate.CaptureIndex);

			PostGrow();

			bSearch = false;
		}
	}

	void FDiffusion::PostGrow()
	{
		// Probe from last captured candidate
		Probe(Captured.Last());

		// Sort candidates
		switch (Config.Sorting)
		{
		case EPCGExFloodFillPrioritization::Heuristics:
			Candidates.Sort([&](const FCandidate& A, const FCandidate& B)
			{
				if (A.Score == B.Score) { return A.Depth > B.Depth; }
				return A.Score > B.Score;
			});
			break;
		case EPCGExFloodFillPrioritization::Depth:
			Candidates.Sort([&](const FCandidate& A, const FCandidate& B)
			{
				if (A.Depth == B.Depth) { return A.Score > B.Score; }
				return A.Depth > B.Depth;
			});
			break;
		}
	}

	void DiffuseAndBlend(
		const FDiffusion& Diffusion,
		const TSharedPtr<PCGExData::FFacade>& InVtxFacade,
		const TSharedPtr<PCGExBlending::FBlendOpsManager>& InBlendOps,
		TArray<int32>& OutIndices)
	{
		OutIndices.SetNumUninitialized(Diffusion.Captured.Num());
		const int32 SourceIndex = Diffusion.SeedNode->PointIndex;

		for (int i = 0; i < OutIndices.Num(); i++)
		{
			const FCandidate& Candidate = Diffusion.Captured[i];
			const int32 TargetIndex = Candidate.Node->PointIndex;

			OutIndices[i] = TargetIndex;

			if (TargetIndex != SourceIndex)
			{
				// TODO : Compute weight based on distance or depth
				InBlendOps->BlendAutoWeight(SourceIndex, TargetIndex);
			}
		}
	}

	FFillControlsHandler::FFillControlsHandler(FPCGExContext* InContext, const TSharedPtr<PCGExClusters::FCluster>& InCluster, const TSharedPtr<PCGExData::FFacade>& InVtxDataCache, const TSharedPtr<PCGExData::FFacade>& InEdgeDataCache, const TSharedPtr<PCGExData::FFacade>& InSeedsDataCache, const TArray<TObjectPtr<const UPCGExFillControlsFactoryData>>& InFactories)
		: ExecutionContext(InContext), Cluster(InCluster), VtxDataFacade(InVtxDataCache), EdgeDataFacade(InEdgeDataCache), SeedsDataFacade(InSeedsDataCache)
	{
		bIsValidHandler = BuildFrom(InContext, InFactories);
	}

	FFillControlsHandler::~FFillControlsHandler()
	{
	}

	bool FFillControlsHandler::BuildFrom(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExFillControlsFactoryData>>& InFactories)
	{
		if (!InFactories.IsEmpty())
		{
			Operations.Reserve(InFactories.Num());

			for (const TObjectPtr<const UPCGExFillControlsFactoryData>& Factory : InFactories)
			{
				TSharedPtr<FPCGExFillControlOperation> Op = Factory->CreateOperation(InContext);
				if (!Op) { return false; }

				Operations.Add(Op);
				if (Op->DoesScoring()) { SubOpsScoring.Add(Op); }
				if (Op->ChecksProbe()) { SubOpsProbe.Add(Op); }
				if (Op->ChecksCandidate()) { SubOpsCandidate.Add(Op); }
				if (Op->ChecksCapture()) { SubOpsCapture.Add(Op); }
			}
		}

		return true;
	}

	bool FFillControlsHandler::PrepareForDiffusions(const TArray<TSharedPtr<FDiffusion>>& Diffusions, const FPCGExFloodFillFlowDetails& Details)
	{
		// Note: HeuristicsHandler is now optional - deprecated node-level heuristics
		// Use 'Heuristics Scoring' fill control instead for modern approach

		NumDiffusions = Diffusions.Num();

		SeedIndices = MakeShared<TArray<int32>>();
		SeedNodeIndices = MakeShared<TArray<int32>>();

		SeedIndices->SetNumUninitialized(NumDiffusions);
		SeedNodeIndices->SetNumUninitialized(NumDiffusions);

		// Create shared config from details
		DiffusionConfig = FDiffusionConfig(Details);

		for (int i = 0; i < NumDiffusions; i++)
		{
			*(SeedIndices->GetData() + i) = Diffusions[i]->SeedIndex;
			*(SeedNodeIndices->GetData() + i) = Diffusions[i]->SeedNode->PointIndex;

			// Set config on each diffusion
			Diffusions[i]->Config = DiffusionConfig;
		}

		PCGEX_SHARED_THIS_DECL
		for (const TSharedPtr<FPCGExFillControlOperation>& Op : Operations)
		{
			Op->SettingsIndex = Op->Factory->ConfigBase.Source == EPCGExFloodFillSettingSource::Seed ? SeedIndices : SeedNodeIndices;
			if (!Op->PrepareForDiffusions(ExecutionContext, ThisPtr)) { return false; }
		}

		return true;
	}

	void FFillControlsHandler::ScoreCandidate(const FDiffusion* Diffusion, const FCandidate& From, FCandidate& OutCandidate)
	{
		for (const TSharedPtr<FPCGExFillControlOperation>& Op : SubOpsScoring)
		{
			Op->ScoreCandidate(Diffusion, From, OutCandidate);
		}
	}

	bool FFillControlsHandler::TryCapture(const FDiffusion* Diffusion, const FCandidate& Candidate)
	{
		for (const TSharedPtr<FPCGExFillControlOperation>& Op : SubOpsCapture) { if (!Op->IsValidCapture(Diffusion, Candidate)) { return false; } }
		if (FPlatformAtomics::InterlockedCompareExchange((InfluencesCount->GetData() + Candidate.Node->PointIndex), 1, 0) == 1) { return false; }
		return true;
	}

	bool FFillControlsHandler::IsValidProbe(const FDiffusion* Diffusion, const FCandidate& Candidate)
	{
		for (const TSharedPtr<FPCGExFillControlOperation>& Op : SubOpsProbe) { if (!Op->IsValidProbe(Diffusion, Candidate)) { return false; } }
		return true;
	}

	bool FFillControlsHandler::IsValidCandidate(const FDiffusion* Diffusion, const FCandidate& From, const FCandidate& Candidate)
	{
		for (const TSharedPtr<FPCGExFillControlOperation>& Op : SubOpsCandidate) { if (!Op->IsValidCandidate(Diffusion, From, Candidate)) { return false; } }
		return true;
	}

#pragma region FDiffusionPathWriter

	FDiffusionPathWriter::FDiffusionPathWriter(
		const TSharedRef<PCGExClusters::FCluster>& InCluster,
		const TSharedRef<PCGExData::FFacade>& InVtxDataFacade,
		const TSharedRef<PCGExData::FPointIOCollection>& InPaths)
		: Cluster(InCluster)
		, VtxDataFacade(InVtxDataFacade)
		, Paths(InPaths)
	{
	}

	void FDiffusionPathWriter::WriteFullPath(
		const FDiffusion& Diffusion,
		const int32 EndpointNodeIndex,
		const FPCGExAttributeToTagDetails& SeedTags,
		const TSharedRef<PCGExData::FFacade>& SeedsDataFacade)
	{
		int32 PathNodeIndex = PCGEx::NH64A(Diffusion.TravelStack->Get(EndpointNodeIndex));
		int32 PathEdgeIndex = -1;

		TArray<int32> PathIndices;
		if (PathNodeIndex != -1)
		{
			PathIndices.Add(Cluster->GetNodePointIndex(EndpointNodeIndex));

			while (PathNodeIndex != -1)
			{
				const int32 CurrentIndex = PathNodeIndex;
				PCGEx::NH64(Diffusion.TravelStack->Get(CurrentIndex), PathNodeIndex, PathEdgeIndex);
				PathIndices.Add(Cluster->GetNodePointIndex(CurrentIndex));
			}
		}

		if (PathIndices.Num() < 2) { return; }

		Algo::Reverse(PathIndices);

		// Create a copy of the final vtx, so we get all the goodies
		TSharedPtr<PCGExData::FPointIO> PathIO = Paths->Emplace_GetRef(VtxDataFacade->Source->GetOut(), PCGExData::EIOInit::New);
		PathIO->DeleteAttribute(PCGExPaths::Labels::ClosedLoopIdentifier);

		(void)PCGExPointArrayDataHelpers::SetNumPointsAllocated(PathIO->GetOut(), PathIndices.Num(), VtxDataFacade->Source->GetIn()->GetAllocatedProperties());
		PathIO->InheritPoints(PathIndices, 0);

		SeedTags.Tag(SeedsDataFacade->GetInPoint(Diffusion.SeedIndex), PathIO);

		PathIO->IOIndex = Diffusion.SeedIndex * 1000000 + VtxDataFacade->Source->IOIndex * 1000000 + EndpointNodeIndex;
	}

	void FDiffusionPathWriter::WritePartitionedPath(
		const FDiffusion& Diffusion,
		TArray<int32>& PathIndices,
		const FPCGExAttributeToTagDetails& SeedTags,
		const TSharedRef<PCGExData::FFacade>& SeedsDataFacade)
	{
		if (PathIndices.Num() < 2) { return; }

		Algo::Reverse(PathIndices);

		// Create a copy of the final vtx, so we get all the goodies
		TSharedPtr<PCGExData::FPointIO> PathIO = Paths->Emplace_GetRef(VtxDataFacade->Source->GetOut(), PCGExData::EIOInit::New);
		PathIO->DeleteAttribute(PCGExPaths::Labels::ClosedLoopIdentifier);

		(void)PCGExPointArrayDataHelpers::SetNumPointsAllocated(PathIO->GetOut(), PathIndices.Num(), VtxDataFacade->Source->GetIn()->GetAllocatedProperties());
		PathIO->InheritPoints(PathIndices, 0);

		SeedTags.Tag(SeedsDataFacade->GetInPoint(Diffusion.SeedIndex), PathIO);

		PathIO->IOIndex = Diffusion.SeedIndex * 1000000 + VtxDataFacade->Source->IOIndex * 1000000 + PathIndices[0];
	}

#pragma endregion
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
