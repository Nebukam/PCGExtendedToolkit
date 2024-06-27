// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/States/PCGExNodeStateOperation.h"

bool UPCGExNodeStateOperation::RequiresDirectProcessing() { return false; }

bool UPCGExNodeStateOperation::PrepareForPoints(const PCGExData::FPointIO* InPointIO)
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

void UPCGExNodeStateOperation::ProcessCandidates(const int32 Index, const FPCGPoint& Point, TArray<PCGExProbing::FCandidate>& Candidates, TSet<uint64>* Stacks, const FVector& ST)
{
}

void UPCGExNodeStateOperation::ProcessNode(const int32 Index, const FPCGPoint& Point, TSet<uint64>* Stacks, const FVector& ST)
{
}

void UPCGExNodeStateOperation::Cleanup()
{
	UniqueEdges.Empty();
	Super::Cleanup();
}

void UPCGExNodeStateOperation::AddEdge(uint64 Edge)
{
	FWriteScopeLock WriteScopeLock(UniqueEdgesLock);
	UniqueEdges.Add(Edge);
}
