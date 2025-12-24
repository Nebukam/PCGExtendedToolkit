// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "SubPoints/PCGExSubPointsInstancedFactory.h"

bool FPCGExSubPointsOperation::PrepareForData(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InTargetFacade, const TSet<FName>* IgnoreAttributeSet)
{
	return true;
}

void FPCGExSubPointsOperation::ProcessSubPoints(const PCGExData::FConstPoint& From, const PCGExData::FConstPoint& To, PCGExData::FScope& Scope, const PCGExPaths::FPathMetrics& Metrics) const
{
}

void UPCGExSubPointsInstancedFactory::CopySettingsFrom(const UPCGExInstancedFactory* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExSubPointsInstancedFactory* TypedOther = Cast<UPCGExSubPointsInstancedFactory>(Other))
	{
		bClosedLoop = TypedOther->bClosedLoop;
		bPreserveTransform = TypedOther->bPreserveTransform;
		bPreservePosition = TypedOther->bPreservePosition;
		bPreserveRotation = TypedOther->bPreserveRotation;
		bPreserveScale = TypedOther->bPreserveScale;
	}
}
