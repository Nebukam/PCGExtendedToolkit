// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/Smoothing/PCGExSmoothingOperation.h"

#include "Data/PCGExPointIO.h"
#include "Data/Blending/PCGExMetadataBlender.h"

void UPCGExSmoothingOperation::SmoothSingle(
	PCGExData::FPointIO* Path,
	PCGExData::FPointRef& Target,
	const double Smoothing,
	const double Influence,
	PCGExDataBlending::FMetadataBlender* MetadataBlender,
	const bool bClosedPath)
{
}
