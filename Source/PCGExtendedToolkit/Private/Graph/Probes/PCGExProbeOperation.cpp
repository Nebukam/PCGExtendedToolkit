// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Probes/PCGExProbeOperation.h"

#include "Graph/Probes/PCGExProbing.h"

bool UPCGExProbeOperation::RequiresDirectProcessing() { return false; }

bool UPCGExProbeOperation::RequiresChainProcessing() { return false; }

bool UPCGExProbeOperation::PrepareForPoints(const TSharedPtr<PCGExData::FPointIO>& InPointIO)
{
	PointIO = InPointIO;

	if (BaseConfig->SearchRadiusInput == EPCGExInputValueType::Constant)
	{
		SearchRadius = BaseConfig->SearchRadiusConstant;
		SearchRadiusSquared = SearchRadius * SearchRadius;
	}
	else
	{
		SearchRadiusCache = PrimaryDataFacade->GetScopedBroadcaster<double>(BaseConfig->SearchRadiusAttribute);

		if (!SearchRadiusCache)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FText::FromString(TEXT("Invalid Radius attribute: \"{0}\"")), FText::FromName(BaseConfig->SearchRadiusAttribute.GetName())));
			return false;
		}
	}

	return true;
}

void UPCGExProbeOperation::ProcessCandidates(const int32 Index, const FPCGPoint& Point, TArray<PCGExProbing::FCandidate>& Candidates, TSet<FInt32Vector>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges)
{
}

void UPCGExProbeOperation::PrepareBestCandidate(const int32 Index, const FPCGPoint& Point, PCGExProbing::FBestCandidate& InBestCandidate)
{
}

void UPCGExProbeOperation::ProcessCandidateChained(const int32 Index, const FPCGPoint& Point, const int32 CandidateIndex, PCGExProbing::FCandidate& Candidate, PCGExProbing::FBestCandidate& InBestCandidate)
{
}

void UPCGExProbeOperation::ProcessBestCandidate(const int32 Index, const FPCGPoint& Point, PCGExProbing::FBestCandidate& InBestCandidate, TArray<PCGExProbing::FCandidate>& Candidates, TSet<FInt32Vector>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges)
{
}

void UPCGExProbeOperation::ProcessNode(const int32 Index, const FPCGPoint& Point, TSet<FInt32Vector>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges)
{
}
