// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Probes/PCGExProbeOperation.h"

#include "Graph/Probes/PCGExProbing.h"

bool UPCGExProbeOperation::RequiresDirectProcessing() { return false; }

bool UPCGExProbeOperation::RequiresChainProcessing() { return false; }

bool UPCGExProbeOperation::PrepareForPoints(const PCGExData::FPointIO* InPointIO)
{
	PointIO = InPointIO;

	if (BaseDescriptor->SearchRadiusSource == EPCGExFetchType::Constant)
	{
		SearchRadiusSquared = BaseDescriptor->SearchRadiusConstant * BaseDescriptor->SearchRadiusConstant;
	}
	else
	{
		SearchRadiusCache = PrimaryDataCache->GetOrCreateGetter<double>(BaseDescriptor->SearchRadiusAttribute);

		if (!SearchRadiusCache)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FText::FromString(TEXT("Invalid Radius attribute: {0}")), FText::FromName(BaseDescriptor->SearchRadiusAttribute.GetName())));
			return false;
		}
	}

	return true;
}

void UPCGExProbeOperation::ProcessCandidates(const int32 Index, const FPCGPoint& Point, TArray<PCGExProbing::FCandidate>& Candidates, TSet<uint64>* ConnectedSet, const FVector& ST, TSet<uint64>* OutEdges)
{
}

void UPCGExProbeOperation::PrepareBestCandidate(const int32 Index, const FPCGPoint& Point, PCGExProbing::FBestCandidate& InBestCandidate)
{
}

void UPCGExProbeOperation::ProcessCandidateChained(const int32 Index, const FPCGPoint& Point, const int32 CandidateIndex, PCGExProbing::FCandidate& Candidate, PCGExProbing::FBestCandidate& InBestCandidate)
{
}

void UPCGExProbeOperation::ProcessBestCandidate(const int32 Index, const FPCGPoint& Point, PCGExProbing::FBestCandidate& InBestCandidate, TArray<PCGExProbing::FCandidate>& Candidates, TSet<uint64>* Stacks, const FVector& ST, TSet<uint64>* OutEdges)
{
}

void UPCGExProbeOperation::ProcessNode(const int32 Index, const FPCGPoint& Point, TSet<uint64>* Stacks, const FVector& ST, TSet<uint64>* OutEdges)
{
}

void UPCGExProbeOperation::Cleanup()
{
	Super::Cleanup();
}
