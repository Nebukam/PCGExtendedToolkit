// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Probes/PCGExProbeOperation.h"


#include "Details/PCGExDetailsSettings.h"
#include "Graph/Probes/PCGExProbing.h"

bool FPCGExProbeOperation::RequiresOctree() { return true; }

bool FPCGExProbeOperation::RequiresChainProcessing() { return false; }

PCGEX_SETTING_VALUE_GET_IMPL(FPCGExProbeConfigBase, SearchRadius, double, SearchRadiusInput, SearchRadiusAttribute, SearchRadiusConstant);

bool FPCGExProbeOperation::PrepareForPoints(FPCGExContext* InContext, const TSharedPtr<PCGExData::FPointIO>& InPointIO)
{
	PointIO = InPointIO;

	SearchRadius = BaseConfig->GetValueSettingSearchRadius();
	if (!SearchRadius->Init(PrimaryDataFacade)) { return false; }
	SearchRadiusOffset = SearchRadius->IsConstant() ? 0 : BaseConfig->SearchRadiusOffset;

	return true;
}

void FPCGExProbeOperation::ProcessCandidates(const int32 Index, const FTransform& WorkingTransform, TArray<PCGExProbing::FCandidate>& Candidates, TSet<FInt32Vector>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges)
{
}

void FPCGExProbeOperation::PrepareBestCandidate(const int32 Index, const FTransform& WorkingTransform, PCGExProbing::FBestCandidate& InBestCandidate)
{
}

void FPCGExProbeOperation::ProcessCandidateChained(const int32 Index, const FTransform& WorkingTransform, const int32 CandidateIndex, PCGExProbing::FCandidate& Candidate, PCGExProbing::FBestCandidate& InBestCandidate)
{
}

void FPCGExProbeOperation::ProcessBestCandidate(const int32 Index, const FTransform& WorkingTransform, PCGExProbing::FBestCandidate& InBestCandidate, TArray<PCGExProbing::FCandidate>& Candidates, TSet<FInt32Vector>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges)
{
}

void FPCGExProbeOperation::ProcessNode(const int32 Index, const FTransform& WorkingTransform, TSet<FInt32Vector>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges, const TArray<int8>& AcceptConnections)
{
}

double FPCGExProbeOperation::GetSearchRadius(const int32 Index) const
{
	return FMath::Square(SearchRadius->Read(Index) + SearchRadiusOffset);
}
