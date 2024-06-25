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
		PCGEx::FLocalSingleFieldGetter* RadiusGetter = new PCGEx::FLocalSingleFieldGetter();
		RadiusGetter->Capture(BaseDescriptor->SearchRadiusAttribute);

		if (!RadiusGetter->Grab(InPointIO))
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FText::FromString(TEXT("Invalid Radius attribute: {0}")), FText::FromName(BaseDescriptor->SearchRadiusAttribute.GetName())));
			PCGEX_DELETE(RadiusGetter)
			return false;
		}

		SearchRadiusCache.Reserve(InPointIO->GetNum());
		SearchRadiusCache.Append(RadiusGetter->Values);
		PCGEX_DELETE(RadiusGetter)
	}

	return true;
}

void UPCGExProbeOperation::ProcessCandidates(const int32 Index, const FPCGPoint& Point, TArray<PCGExProbing::FCandidate>& Candidates)
{
}

void UPCGExProbeOperation::ProcessNode(const int32 Index, const FPCGPoint& Point)
{
}

void UPCGExProbeOperation::Cleanup()
{
	SearchRadiusCache.Empty();
	UniqueEdges.Empty();
	Super::Cleanup();
}

void UPCGExProbeOperation::AddEdge(uint64 Edge)
{
	FWriteScopeLock WriteScopeLock(UniqueEdgesLock);
	UniqueEdges.Add(Edge);
}
