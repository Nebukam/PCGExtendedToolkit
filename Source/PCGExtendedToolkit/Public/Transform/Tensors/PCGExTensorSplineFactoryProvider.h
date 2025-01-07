// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "PCGExTensor.h"
#include "PCGExTensorFactoryProvider.h"
#include "Data/PCGSplineStruct.h"
#include "Paths/PCGExPaths.h"
#include "Sampling/PCGExSampleNearestSpline.h"

#include "PCGExTensorSplineFactoryProvider.generated.h"

class UPCGExTensorOperation;

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExTensorSplineFactoryData : public UPCGExTensorFactoryData
{
	GENERATED_BODY()

public:
	FPCGExPathClosedLoopDetails ClosedLoop;
	EPCGExSplinePointTypeRedux PointType = EPCGExSplinePointTypeRedux::Linear;
	bool bBuildFromPaths = false;

protected:
	TArray<TSharedPtr<const FPCGSplineStruct>> ManagedSplines;
	TArray<FPCGSplineStruct> Splines;

	EPCGExSplineSamplingIncludeMode SampleInputs = EPCGExSplineSamplingIncludeMode::All;

	virtual bool GetRequiresPreparation(FPCGExContext* InContext) override { return true; }
	virtual bool InitInternalData(FPCGExContext* InContext) override;
	virtual bool InitInternalFacade(FPCGExContext* InContext);

	virtual void BeginDestroy() override;
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExTensorSplineFactoryProviderSettings : public UPCGExTensorFactoryProviderSettings
{
	GENERATED_BODY()

protected:
	virtual bool GetBuildFromPoints() const { return false; }
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
};
