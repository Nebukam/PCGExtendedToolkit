// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/Smoothing/PCGExSmoothingOperation.h"

#include "Data/PCGExPointIO.h"
#include "Data/Blending/PCGExMetadataBlender.h"
#include "Data/Blending/PCGExPropertiesBlender.h"

void UPCGExSmoothingOperation::DoSmooth(PCGExData::FPointIO& InPointIO)
{
	PCGEx::FLocalSingleFieldGetter InfluenceGetter;

	InPointIO.CreateInKeys();
	InPointIO.CreateOutKeys();
	
	if (bUseLocalInfluence)
	{
		InfluenceGetter.bEnabled = true;
		InfluenceGetter.Capture(InfluenceDescriptor);
		InfluenceGetter.Bind(InPointIO);
	}
	else
	{
		InfluenceGetter.bEnabled = false;
	}

	PCGExDataBlending::FMetadataBlender* MetadataLerp = new PCGExDataBlending::FMetadataBlender(InfluenceSettings.DefaultBlending);
	PCGExDataBlending::FPropertiesBlender* PropertiesLerp = new PCGExDataBlending::FPropertiesBlender(InfluenceSettings);
	MetadataLerp->PrepareForData(&InPointIO, InfluenceSettings.AttributesOverrides);

	InternalDoSmooth(
		InfluenceGetter,
		MetadataLerp,
		PropertiesLerp,
		InPointIO);

	if (bPinStart)
	{
		constexpr int32 Index = 0;
		MetadataLerp->Blend(Index, Index, Index, 1);
		PropertiesLerp->Blend(InPointIO.GetOutPoint(Index), InPointIO.GetInPoint(Index), InPointIO.GetMutablePoint(Index), 1);
	}

	if (bPinEnd)
	{
		const int32 Index = InPointIO.GetNum() - 1;
		MetadataLerp->Blend(Index, Index, Index, 1);
		PropertiesLerp->Blend(InPointIO.GetOutPoint(Index), InPointIO.GetInPoint(Index), InPointIO.GetMutablePoint(Index), 1);
	}

	PCGEX_DELETE(MetadataLerp)
	PCGEX_DELETE(PropertiesLerp)

	InfluenceGetter.Cleanup();
}

double UPCGExSmoothingOperation::GetReverseInfluence(
	const PCGEx::FLocalSingleFieldGetter& Getter, const int32 Index) const
{
	if (!bUseLocalInfluence) { return 1 - FMath::Clamp(FixedInfluence, 0, 1); }
	return FMath::Clamp(1 - Getter.SafeGet(Index, FixedInfluence), 0, 1);
}

void UPCGExSmoothingOperation::InternalDoSmooth(
	const PCGEx::FLocalSingleFieldGetter& InfluenceGetter,
	PCGExDataBlending::FMetadataBlender* MetadataInfluence,
	PCGExDataBlending::FPropertiesBlender* PropertiesInfluence,
	PCGExData::FPointIO& InPointIO)
{
}
