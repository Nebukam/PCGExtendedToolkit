// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Splines/SubPoints/DataBlending/PCGExSubPointsDataBlend.h"

#include "Data/PCGExPointIO.h"
#include "Data/Blending/PCGExMetadataBlender.h"


EPCGExMetadataBlendingOperationType UPCGExSubPointsDataBlend::GetDefaultBlending()
{
	return EPCGExMetadataBlendingOperationType::Copy;
}

void UPCGExSubPointsDataBlend::PrepareForData(const UPCGExPointIO* InData)
{
	if (!Blender)
	{
		Blender = NewObject<UPCGExMetadataBlender>();
		Blender->DefaultOperation = GetDefaultBlending();
	}

	Blender->PrepareForData(InData->Out, BlendingOverrides);
	
	Super::PrepareForData(InData);
}

void UPCGExSubPointsDataBlend::ProcessSubPoints(const FPCGPoint& StartPoint, const FPCGPoint& EndPoint, TArrayView<FPCGPoint>& SubPoints, const double PathLength) const
{
}
