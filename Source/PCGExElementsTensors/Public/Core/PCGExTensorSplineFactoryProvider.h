// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExTensorFactoryProvider.h"
#include "Data/PCGSplineStruct.h"
#include "Filters/Points/PCGExPolyPathFilterFactory.h"

#include "PCGExTensorSplineFactoryProvider.generated.h"

class PCGExTensorOperation;

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXELEMENTSTENSORS_API UPCGExTensorSplineFactoryData : public UPCGExTensorFactoryData
{
	GENERATED_BODY()

public:
	EPCGExSplinePointTypeRedux PointType = EPCGExSplinePointTypeRedux::Linear;
	bool bSmoothLinear = true;
	bool bBuildFromPaths = false;

protected:
	TArray<TSharedPtr<const FPCGSplineStruct>> ManagedSplines;
	TArray<FPCGSplineStruct> Splines;

	EPCGExSplineSamplingIncludeMode SampleInputs = EPCGExSplineSamplingIncludeMode::All;

	virtual bool WantsPreparation(FPCGExContext* InContext) override { return true; }
	virtual PCGExFactories::EPreparationResult InitInternalData(FPCGExContext* InContext) override;
	virtual bool InitInternalFacade(FPCGExContext* InContext);

	virtual void BeginDestroy() override;
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXELEMENTSTENSORS_API UPCGExTensorSplineFactoryProviderSettings : public UPCGExTensorFactoryProviderSettings
{
	GENERATED_BODY()

protected:
	virtual bool GetBuildFromPoints() const { return false; }
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
};
