// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "SubPoints/DataBlending/PCGExSubPointsBlendInheritEnd.h"


#include "Blenders/PCGExMetadataBlender.h"
#include "Data/PCGExPointElements.h"


void FPCGExSubPointsBlendInheritEnd::BlendSubPoints(const PCGExData::FConstPoint& From, const PCGExData::FConstPoint& To, PCGExData::FScope& Scope, const PCGExPaths::FPathMetrics& Metrics) const
{
	TPCGValueRange<FTransform> OutTransform = Scope.Data->GetTransformValueRange(false);

	PCGEX_SCOPE_LOOP(Index)
	{
		//FVector Location = OutTransform[Index].GetLocation();
		MetadataBlender->Blend(From.Index, To.Index, Index, 1);
		//OutTransform[Index].SetLocation(Location);
	}
}

TSharedPtr<FPCGExSubPointsBlendOperation> UPCGExSubPointsBlendInheritEnd::CreateOperation() const
{
	PCGEX_CREATE_SUBPOINTBLEND_OPERATION(InheritEnd)
	return NewOperation;
}
