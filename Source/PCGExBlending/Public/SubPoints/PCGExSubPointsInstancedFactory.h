// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Factories/PCGExInstancedFactory.h"
#include "Factories/PCGExOperation.h"

#include "PCGExSubPointsInstancedFactory.generated.h"

namespace PCGExPaths
{
	struct FPathMetrics;
}

namespace PCGExData
{
	struct FConstPoint;
	struct FScope;
}

class UPCGExSubPointsInstancedFactory;

class FPCGExSubPointsOperation : public FPCGExOperation
{
public:
	const UPCGExSubPointsInstancedFactory* Factory = nullptr;

	bool bClosedLoop = false;
	bool bPreserveTransform = false;
	bool bPreservePosition = false;
	bool bPreserveRotation = false;
	bool bPreserveScale = false;

	virtual bool PrepareForData(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InTargetFacade, const TSet<FName>* IgnoreAttributeSet);

	virtual void ProcessSubPoints(const PCGExData::FConstPoint& From, const PCGExData::FConstPoint& To, PCGExData::FScope& Scope, const PCGExPaths::FPathMetrics& Metrics) const;
};

/**
 * 
 */
UCLASS(Abstract)
class PCGEXBLENDING_API UPCGExSubPointsInstancedFactory : public UPCGExInstancedFactory
{
	GENERATED_BODY()

public:
	bool bClosedLoop = false;
	bool bPreserveTransform = false;
	bool bPreservePosition = false;
	bool bPreserveRotation = false;
	bool bPreserveScale = false;

	virtual void CopySettingsFrom(const UPCGExInstancedFactory* Other) override;

	//virtual TSharedPtr<FPCGExSubPointsOperation> CreateOperation(FPCGExContext* InContext) const;
};
