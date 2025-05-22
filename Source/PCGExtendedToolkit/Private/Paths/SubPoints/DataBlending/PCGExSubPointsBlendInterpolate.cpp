// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"


void FPCGExSubPointsBlendInterpolate::BlendSubPoints(
	const PCGExData::FConstPoint& From, const PCGExData::FConstPoint& To,
	PCGExData::FScope& Scope, const PCGExPaths::FPathMetrics& Metrics, const int32 StartIndex) const
{
	EPCGExBlendOver SafeBlendOver = TypedFactory->BlendOver;
	if (TypedFactory->BlendOver == EPCGExBlendOver::Distance && !Metrics.IsValid()) { SafeBlendOver = EPCGExBlendOver::Index; }

	if (SafeBlendOver == EPCGExBlendOver::Distance)
	{
		PCGExPaths::FPathMetrics PathMetrics = PCGExPaths::FPathMetrics(From.GetLocation());
		for (int i = 0; i < Scope.Num(); i++)
		{
			FVector Location = Scope[i].Transform.GetLocation();
			MetadataBlender->Blend(From.Index, To.Index, StartIndex < 0 ? From.Index : StartIndex, Metrics.GetTime(PathMetrics.Add(Location)));
			Scope[i].Transform.SetLocation(Location);
		}
	}
	else if (SafeBlendOver == EPCGExBlendOver::Index)
	{
		const double Divider = Scope.Num();
		for (int i = 0; i < Scope.Num(); i++)
		{
			FVector Location = Scope[i].Transform.GetLocation();
			MetadataBlender->Blend(From.Index, To.Index, StartIndex < 0 ? From.Index : StartIndex, i / Divider);
			Scope[i].Transform.SetLocation(Location);
		}
	}
	else if (SafeBlendOver == EPCGExBlendOver::Fixed)
	{
		for (int i = 0; i < Scope.Num(); i++)
		{
			FVector Location = Scope[i].Transform.GetLocation();
			MetadataBlender->Blend(From.Index, To.Index, StartIndex < 0 ? From.Index : StartIndex, Lerp);
			Scope[i].Transform.SetLocation(Location);
		}
	}
}

void UPCGExSubPointsBlendInterpolate::CopySettingsFrom(const UPCGExInstancedFactory* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExSubPointsBlendInterpolate* TypedOther = Cast<UPCGExSubPointsBlendInterpolate>(Other))
	{
		BlendOver = TypedOther->BlendOver;
		Lerp = TypedOther->Lerp;
	}
}

TSharedPtr<FPCGExSubPointsBlendOperation> UPCGExSubPointsBlendInterpolate::CreateOperation() const
{
	PCGEX_CREATE_SUBPOINTBLEND_OPERATION(Interpolate)
	NewOperation->TypedFactory = this;
	NewOperation->Lerp = Lerp;
	return NewOperation;
}
