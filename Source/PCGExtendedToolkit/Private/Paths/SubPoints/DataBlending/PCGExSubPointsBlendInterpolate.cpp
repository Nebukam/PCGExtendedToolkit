// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"


EPCGExDataBlendingType UPCGExSubPointsBlendInterpolate::GetDefaultBlending()
{
	return EPCGExDataBlendingType::Lerp;
}

void UPCGExSubPointsBlendInterpolate::ApplyOverrides()
{
	Super::ApplyOverrides();
	PCGEX_OVERRIDE_OP_PROPERTY(Lerp, FName(TEXT("Blending/Weight")), EPCGMetadataTypes::Double);
}

void UPCGExSubPointsBlendInterpolate::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExSubPointsBlendInterpolate* TypedOther = Cast<UPCGExSubPointsBlendInterpolate>(Other))
	{
		BlendOver = TypedOther->BlendOver;
		Lerp = TypedOther->Lerp;
	}
}

void UPCGExSubPointsBlendInterpolate::BlendSubPoints(
	const PCGExData::FPointRef& From,
	const PCGExData::FPointRef& To,
	const TArrayView<FPCGPoint>& SubPoints,
	const PCGExPaths::FPathMetrics& Metrics,
	PCGExDataBlending::FMetadataBlender* InBlender,
	const int32 StartIndex) const
{
	const int32 NumPoints = SubPoints.Num();

	EPCGExBlendOver SafeBlendOver = BlendOver;
	if (BlendOver == EPCGExBlendOver::Distance && !Metrics.IsValid()) { SafeBlendOver = EPCGExBlendOver::Index; }

	TArray<double> Weights;
	TArray<FVector> Locations;

	Weights.SetNumUninitialized(NumPoints);
	Locations.SetNumUninitialized(NumPoints);

	if (SafeBlendOver == EPCGExBlendOver::Distance)
	{
		PCGExPaths::FPathMetrics PathMetrics = PCGExPaths::FPathMetrics(From.Point->Transform.GetLocation());
		for (int i = 0; i < NumPoints; i++)
		{
			const FVector Location = SubPoints[i].Transform.GetLocation();
			Locations[i] = Location;
			Weights[i] = Metrics.GetTime(PathMetrics.Add(Location));
		}
	}
	else if (SafeBlendOver == EPCGExBlendOver::Index)
	{
		for (int i = 0; i < NumPoints; i++)
		{
			Locations[i] = SubPoints[i].Transform.GetLocation();
			Weights[i] = static_cast<double>(i) / NumPoints;
		}
	}
	else if (SafeBlendOver == EPCGExBlendOver::Fixed)
	{
		for (int i = 0; i < NumPoints; i++)
		{
			Locations[i] = SubPoints[i].Transform.GetLocation();
			Weights[i] = Lerp;
		}
	}

	InBlender->BlendRangeFromTo(From, To, StartIndex < 0 ? From.Index : StartIndex, Weights);

	// Restore pre-blend position
	for (int i = 0; i < NumPoints; i++) { SubPoints[i].Transform.SetLocation(Locations[i]); }
}

TSharedPtr<PCGExDataBlending::FMetadataBlender> UPCGExSubPointsBlendInterpolate::CreateBlender(
	const TSharedRef<PCGExData::FFacade>& InPrimaryFacade,
	const TSharedRef<PCGExData::FFacade>& InSecondaryFacade,
	const PCGExData::ESource SecondarySource,
	const TSet<FName>* IgnoreAttributeSet)
{
	TSharedPtr<PCGExDataBlending::FMetadataBlender> NewBlender = MakeShared<PCGExDataBlending::FMetadataBlender>(&BlendingDetails);
	NewBlender->PrepareForData(InPrimaryFacade, InSecondaryFacade, SecondarySource, true, IgnoreAttributeSet);
	return NewBlender;
}
