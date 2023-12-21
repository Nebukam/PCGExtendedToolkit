// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/Smoothing/PCGExSmoothingOperation.h"

#include "Data/PCGExPointIO.h"
#include "Data/Blending/PCGExMetadataBlender.h"
#include "Data/Blending/PCGExPropertiesBlender.h"

void UPCGExSmoothingOperation::DoSmooth(PCGExData::FPointIO& InPointIO)
{
	InPointIO.CreateInKeys();
	InPointIO.CreateOutKeys();


	InternalDoSmooth(InPointIO);

	//Influence pass

	TArray<double> Influences;
	Influences.SetNumZeroed(InPointIO.GetNum());

	PCGEx::FLocalSingleFieldGetter InfluenceGetter;

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

	if (!bUseLocalInfluence || !InfluenceGetter.bValid)
	{
		const double StaticInfluence = 1 - FMath::Clamp(FixedInfluence, 0, 1);
		for (int i = 0; i < Influences.Num(); i++) { Influences[i] = StaticInfluence; }
	}
	else
	{
		for (int i = 0; i < Influences.Num(); i++) { Influences[i] = FMath::Clamp(1 - InfluenceGetter.Values[i], 0, 1); }
	}

	if (bPinStart) { Influences[0] = 1; }
	if (bPinEnd) { Influences[InPointIO.GetNum() - 1] = 1; }

	PCGExDataBlending::FMetadataBlender* MetadataLerp = new PCGExDataBlending::FMetadataBlender(InfluenceSettings.DefaultBlending);
	MetadataLerp->PrepareForData(InPointIO, InfluenceSettings.AttributesOverrides);
	MetadataLerp->FullBlendToOne(Influences);
	MetadataLerp->Write();

	PCGExDataBlending::FPropertiesBlender* PropertiesLerp = new PCGExDataBlending::FPropertiesBlender(InfluenceSettings);
	const TArray<FPCGPoint>& InPoints = InPointIO.GetIn()->GetPoints();
	TArray<FPCGPoint>& OutPoints = InPointIO.GetOut()->GetMutablePoints();

	for (int i = 0; i < InPointIO.GetNum(); i++)
	{
		PropertiesLerp->Blend(OutPoints[i], InPoints[i], OutPoints[i], Influences[i]);
	}

	PCGEX_DELETE(MetadataLerp)
	PCGEX_DELETE(PropertiesLerp)

	Influences.Empty();
	InfluenceGetter.Cleanup();
}

void UPCGExSmoothingOperation::InternalDoSmooth(
	PCGExData::FPointIO& InPointIO)
{
}
