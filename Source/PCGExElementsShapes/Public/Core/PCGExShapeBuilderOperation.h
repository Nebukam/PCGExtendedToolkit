// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExShapeConfigBase.h"
#include "Factories/PCGExOperation.h"

namespace PCGExData
{
	struct FScope;
	struct FConstPoint;
}

namespace PCGExShapes
{
	class FShape;
}

/**
 * 
 */
class PCGEXELEMENTSSHAPES_API FPCGExShapeBuilderOperation : public FPCGExOperation
{
public:
	virtual bool PrepareForSeeds(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InSeedDataFacade);

	TArray<TSharedPtr<PCGExShapes::FShape>> Shapes;
	FTransform Transform;
	FPCGExShapeConfigBase BaseConfig;

	virtual void PrepareShape(const PCGExData::FConstPoint& Seed);

	virtual void BuildShape(const TSharedPtr<PCGExShapes::FShape> InShape, TSharedPtr<PCGExData::FFacade> InDataFacade, const PCGExData::FScope& Scope, const bool bIsolated = false)
	{
	}

protected:
	virtual void ValidateShape(const TSharedPtr<PCGExShapes::FShape> Shape);

	double GetResolution(const PCGExData::FConstPoint& Seed) const;

	TSharedPtr<PCGExDetails::TSettingValue<double>> Resolution;
	TSharedPtr<PCGExData::FFacade> SeedFacade;
};
