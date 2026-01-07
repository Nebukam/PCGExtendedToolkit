// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/FloodFill/PCGExFloodFill.h"

#include "PCGExHeuristicsHandler.h"
#include "Clusters/PCGExCluster.h"
#include "Containers/PCGExHashLookup.h"
#include "Core/PCGExBlendOpsManager.h"

#include "Elements/FloodFill/FillControls/PCGExFillControlOperation.h"
#include "Elements/FloodFill/FillControls/PCGExFillControlsFactoryProvider.h"

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

		// Gather all neighbors and compute heuristics, add to candidate for the first time only
		bool bIsAlreadyInSet = false;

		TSharedPtr<PCGExHeuristics::FHandler> HeuristicsHandler = FillControlsHandler->HeuristicsHandler.Pin();
		if (!HeuristicsHandler) { return; }

		const PCGExClusters::FNode& FromNode = *From.Node;
		const PCGExClusters::FNode& RoamingGoal = *HeuristicsHandler->GetRoamingGoal();

		FVector FromPosition = Cluster->GetPos(FromNode);

		for (const PCGExGraphs::FLink& Lk : FromNode.Links)
		{
			PCGExClusters::FNode* OtherNode = Cluster->GetNode(Lk);
			Visited.Add(OtherNode->Index, &bIsAlreadyInSet);

			if (bIsAlreadyInSet) { continue; }

			FVector OtherPosition = Cluster->GetPos(OtherNode);
			double Dist = FVector::Dist(FromPosition, OtherPosition);

			const double LocalScore = HeuristicsHandler->GetEdgeScore(FromNode, *OtherNode, *Cluster->GetEdge(Lk), *SeedNode, RoamingGoal, nullptr, TravelStack);

			FCandidate Candidate = FCandidate{};
			Candidate.CaptureIndex = From.CaptureIndex;

			Candidate.Link = PCGExGraphs::FLink(FromNode.Index, Lk.Edge);
			Candidate.Node = OtherNode;

			if (FillControlsHandler->bUseLocalScore || FillControlsHandler->bUsePreviousScore)
			{
				if (FillControlsHandler->bUsePreviousScore)
				{
					Candidate.PathScore = From.PathScore + LocalScore;
					Candidate.Score += From.PathScore;
				}

				if (FillControlsHandler->bUseLocalScore)
				{
					Candidate.Score += LocalScore;
				}
			}

			if (FillControlsHandler->bUseGlobalScore) { Candidate.Score += HeuristicsHandler->GetGlobalScore(FromNode, *SeedNode, *OtherNode); }

			Candidate.Depth = From.Depth + 1;
			Candidate.Distance = Dist;
			Candidate.PathDistance = From.PathDistance + Dist;

			if (FillControlsHandler->IsValidCandidate(this, From, Candidate))
			{
				// Valid candidate
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

		switch (FillControlsHandler->Sorting)
		{
		case EPCGExFloodFillPrioritization::Heuristics: Candidates.Sort([&](const FCandidate& A, const FCandidate& B)
			{
				if (A.Score == B.Score) { return A.Depth > B.Depth; }
				return A.Score > B.Score;
			});
			break;
		case EPCGExFloodFillPrioritization::Depth: Candidates.Sort([&](const FCandidate& A, const FCandidate& B)
			{
				if (A.Depth == B.Depth) { return A.Score > B.Score; }
				return A.Depth > B.Depth;
			});
			break;
		}
	}

	void FDiffusion::Diffuse(const TSharedPtr<PCGExData::FFacade>& InVtxFacade, const TSharedPtr<PCGExBlending::FBlendOpsManager>& InBlendOps, TArray<int32>& OutIndices)
	{
		OutIndices.SetNumUninitialized(Captured.Num());
		const int32 SourceIndex = SeedNode->PointIndex;

		for (int i = 0; i < OutIndices.Num(); i++)
		{
			const FCandidate& Candidate = Captured[i];
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
				if (Op->ChecksProbe()) { SubOpsProbe.Add(Op); }
				if (Op->ChecksCandidate()) { SubOpsCandidate.Add(Op); }
				if (Op->ChecksCapture()) { SubOpsCapture.Add(Op); }
			}
		}

		return true;
	}

	bool FFillControlsHandler::PrepareForDiffusions(const TArray<TSharedPtr<FDiffusion>>& Diffusions, const FPCGExFloodFillFlowDetails& Details)
	{
		check(HeuristicsHandler.Pin())

		NumDiffusions = Diffusions.Num();

		SeedIndices = MakeShared<TArray<int32>>();
		SeedNodeIndices = MakeShared<TArray<int32>>();

		SeedIndices->SetNumUninitialized(NumDiffusions);
		SeedNodeIndices->SetNumUninitialized(NumDiffusions);

		for (int i = 0; i < NumDiffusions; i++)
		{
			*(SeedIndices->GetData() + i) = Diffusions[i]->SeedIndex;
			*(SeedNodeIndices->GetData() + i) = Diffusions[i]->SeedNode->PointIndex;
		}

		Sorting = Details.Priority;

		bUseLocalScore = (Details.Scoring & static_cast<uint8>(EPCGExFloodFillHeuristicFlags::LocalScore)) != 0;
		bUseGlobalScore = (Details.Scoring & static_cast<uint8>(EPCGExFloodFillHeuristicFlags::GlobalScore)) != 0;
		bUsePreviousScore = (Details.Scoring & static_cast<uint8>(EPCGExFloodFillHeuristicFlags::PreviousScore)) != 0;

		PCGEX_SHARED_THIS_DECL
		for (const TSharedPtr<FPCGExFillControlOperation>& Op : Operations)
		{
			Op->SettingsIndex = Op->Factory->ConfigBase.Source == EPCGExFloodFillSettingSource::Seed ? SeedIndices : SeedNodeIndices;
			if (!Op->PrepareForDiffusions(ExecutionContext, ThisPtr)) { return false; }
		}

		return true;
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
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
