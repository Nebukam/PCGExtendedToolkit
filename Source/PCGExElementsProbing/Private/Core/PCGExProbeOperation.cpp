// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Core/PCGExProbeOperation.h"


#include "Details/PCGExSettingsDetails.h"
#include "Core/PCGExProbingCandidates.h"
#include "Data/PCGExData.h"

bool FPCGExProbeOperation::IsDirectProbe() const { return false; }

bool FPCGExProbeOperation::RequiresChainProcessing() const { return false; }

PCGEX_SETTING_VALUE_IMPL(FPCGExProbeConfigBase, SearchRadius, double, SearchRadiusInput, SearchRadiusAttribute, SearchRadiusConstant)

bool FPCGExProbeOperation::Prepare(FPCGExContext* InContext)
{
	PointIO = PrimaryDataFacade->Source;

	SearchRadius = BaseConfig->GetValueSettingSearchRadius();
	if (!SearchRadius->Init(PrimaryDataFacade)) { return false; }
	SearchRadiusOffset = SearchRadius->IsConstant() ? 0 : BaseConfig->SearchRadiusOffset;

	return true;
}

void FPCGExProbeOperation::ProcessCandidates(const int32 Index, TArray<PCGExProbing::FCandidate>& Candidates, TSet<uint64>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges, PCGExMT::FScopedContainer* Container)
{
}

bool FPCGExProbeOperation::IsGlobalProbe() const
{
	return false;
}

bool FPCGExProbeOperation::WantsOctree() const
{
	return false;
}

void FPCGExProbeOperation::PrepareBestCandidate(const int32 Index, PCGExProbing::FBestCandidate& InBestCandidate, PCGExMT::FScopedContainer* Container)
{
}

void FPCGExProbeOperation::ProcessCandidateChained(const int32 Index, const int32 CandidateIndex, PCGExProbing::FCandidate& Candidate, PCGExProbing::FBestCandidate& InBestCandidate, PCGExMT::FScopedContainer* Container)
{
}

void FPCGExProbeOperation::ProcessBestCandidate(const int32 Index, PCGExProbing::FBestCandidate& InBestCandidate, TArray<PCGExProbing::FCandidate>& Candidates, TSet<uint64>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges, PCGExMT::FScopedContainer* Container)
{
}

void FPCGExProbeOperation::ProcessNode(const int32 Index, TSet<uint64>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges, PCGExMT::FScopedContainer* Container)
{
}

void FPCGExProbeOperation::ProcessAll(TSet<uint64>& OutEdges) const
{
}

double FPCGExProbeOperation::GetSearchRadius(const int32 Index) const
{
	return FMath::Square(SearchRadius->Read(Index) + SearchRadiusOffset);
}
