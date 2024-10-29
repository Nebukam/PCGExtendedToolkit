// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Paths/SubPoints/PCGExSubPointsOperation.h"
#include "PCGExOrientOperation.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, Abstract)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExOrientOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	EPCGExAxis OrientAxis = EPCGExAxis::Forward;
	EPCGExAxis UpAxis = EPCGExAxis::Up;

	TSharedPtr<PCGExPaths::FPath> Path;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override
	{
		Super::CopySettingsFrom(Other);
		if (const UPCGExOrientOperation* TypedOther = Cast<UPCGExOrientOperation>(Other))
		{
			OrientAxis = TypedOther->OrientAxis;
			UpAxis = TypedOther->UpAxis;
		}
	}

	virtual bool PrepareForData(const TSharedRef<PCGExData::FFacade>& InDataFacade, const TSharedRef<PCGExPaths::FPath>& InPath)
	{
		Path = InPath;
		return true;
	}

	virtual FTransform ComputeOrientation(
		const PCGExData::FPointRef& Point,
		const double DirectionMultiplier) const
	{
		return Point.Point->Transform;
	}
};
