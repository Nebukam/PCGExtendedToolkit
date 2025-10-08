﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Data/PCGExPointElements.h"
#include "Paths/SubPoints/PCGExSubPointsInstancedFactory.h"

#include "PCGExOrientOperation.generated.h"

class UPCGExOrientInstancedFactory;

class FPCGExOrientOperation : public FPCGExOperation
{
public:
	const UPCGExOrientInstancedFactory* Factory = nullptr;
	TSharedPtr<PCGExPaths::FPath> Path;

	virtual bool PrepareForData(const TSharedRef<PCGExData::FFacade>& InDataFacade, const TSharedRef<PCGExPaths::FPath>& InPath)
	{
		Path = InPath;
		return true;
	}

	virtual FTransform ComputeOrientation(
		const PCGExData::FConstPoint& Point,
		const double DirectionMultiplier) const
	{
		return Point.GetTransform();
	}
};

/**
 * 
 */
UCLASS(Abstract)
class PCGEXTENDEDTOOLKIT_API UPCGExOrientInstancedFactory : public UPCGExInstancedFactory
{
	GENERATED_BODY()

public:
	EPCGExAxis OrientAxis = EPCGExAxis::Forward;
	EPCGExAxis UpAxis = EPCGExAxis::Up;

	virtual void CopySettingsFrom(const UPCGExInstancedFactory* Other) override
	{
		Super::CopySettingsFrom(Other);
		if (const UPCGExOrientInstancedFactory* TypedOther = Cast<UPCGExOrientInstancedFactory>(Other))
		{
			OrientAxis = TypedOther->OrientAxis;
			UpAxis = TypedOther->UpAxis;
		}
	}

	virtual TSharedPtr<FPCGExOrientOperation> CreateOperation() const
	PCGEX_NOT_IMPLEMENTED_RET(CreateOperation(), nullptr);
};
