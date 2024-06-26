// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Probes/PCGExProbeOperation.h"

#include "Graph/Probes/PCGExProbing.h"

bool UPCGExProbeOperation::RequiresDirectProcessing() { return false; }

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

void UPCGExProbeOperation::ProcessCandidates(const int32 Index, const FPCGPoint& Point, TArray<PCGExProbing::FCandidate>& Candidates, TSet<uint64>* Stacks, const FVector& ST)
{
}

void UPCGExProbeOperation::ProcessNode(const int32 Index, const FPCGPoint& Point, TSet<uint64>* Stacks, const FVector& ST)
{
}

void UPCGExProbeOperation::Cleanup()
{
	UniqueEdges.Empty();
	Super::Cleanup();
}

void UPCGExProbeOperation::AddEdge(uint64 Edge)
{
	FWriteScopeLock WriteScopeLock(UniqueEdgesLock);
	UniqueEdges.Add(Edge);
}
