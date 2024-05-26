// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/Smoothing/PCGExSmoothingOperation.h"

#include "Data/PCGExPointIO.h"
#include "Data/Blending/PCGExMetadataBlender.h"

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
		InfluenceGetter.Grab(InPointIO);
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

	if (bPreserveStart) { Influences[0] = 1; }
	if (bPreserveEnd) { Influences[InPointIO.GetNum() - 1] = 1; }

	PCGExDataBlending::FMetadataBlender* MetadataLerp = new PCGExDataBlending::FMetadataBlender(&InfluenceSettings);
	MetadataLerp->PrepareForData(InPointIO);
	MetadataLerp->BlendEachPrimaryToSecondary(Influences);
	MetadataLerp->Write();

	PCGEX_DELETE(MetadataLerp)

	Influences.Empty();
	InfluenceGetter.Cleanup();
}

void UPCGExSmoothingOperation::InternalDoSmooth(
	PCGExData::FPointIO& InPointIO)
{
}
