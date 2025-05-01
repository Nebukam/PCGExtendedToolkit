// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDetailsData.h"
#include "UObject/Object.h"
#include "PCGExOperation.h"
#include "PCGExShapes.h"
#include "PCGExShapeBuilderOperation.generated.h"

/**
 * 
 */
UCLASS(Abstract)
class PCGEXTENDEDTOOLKIT_API UPCGExShapeBuilderOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;
	virtual bool PrepareForSeeds(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InSeedDataFacade);

	virtual void Cleanup() override;
	
	TArray<TSharedPtr<PCGExShapes::FShape>> Shapes;
	FTransform Transform;
	FPCGExShapeConfigBase BaseConfig;

	virtual void PrepareShape(const PCGExData::FPointRef& Seed) { Shapes[Seed.Index] = MakeShared<PCGExShapes::FShape>(Seed); }

	virtual void BuildShape(const TSharedPtr<PCGExShapes::FShape> InShape, TSharedPtr<PCGExData::FFacade> InDataFacade, const TArrayView<FPCGPoint> PointView)
	{
	}

protected:
	virtual void ValidateShape(const TSharedPtr<PCGExShapes::FShape> Shape);
	
	FORCEINLINE double GetResolution(const PCGExData::FPointRef& Seed) const
	{
		if (BaseConfig.ResolutionMode == EPCGExResolutionMode::Distance) { return FMath::Abs(Resolution->Read(Seed.Index)) * 0.01; }
		return FMath::Abs(Resolution->Read(Seed.Index));
	}

	TSharedPtr<PCGExDetails::TSettingValue<double>> Resolution;
	TSharedPtr<PCGExData::FFacade> SeedFacade;
};
