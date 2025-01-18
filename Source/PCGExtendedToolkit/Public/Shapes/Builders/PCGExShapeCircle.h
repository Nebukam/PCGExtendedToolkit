// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "Shapes/PCGExShapeBuilderFactoryProvider.h"
#include "Shapes/PCGExShapeBuilderOperation.h"

#include "PCGExShapeCircle.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExShapeCircleConfig : public FPCGExShapeConfigBase
{
	GENERATED_BODY()

	FPCGExShapeCircleConfig() :
		FPCGExShapeConfigBase()
	{
	}

	/** Start angle source. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExInputValueType StartAngleInput = EPCGExInputValueType::Constant;

	/** Start angle constant, in degrees. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Start Angle", EditCondition="StartAngleInput == EPCGExInputValueType::Constant", EditConditionHides, Units="Degrees"))
	double StartAngleConstant = 0;

	/** Start angle attribute, in degrees. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Start Angle", EditCondition="StartAngleInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector StartAngleAttribute;


	/** End angle source. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExInputValueType EndAngleInput = EPCGExInputValueType::Constant;

	/** End angle constant, in degrees. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="End Angle", EditCondition="EndAngleInput == EPCGExInputValueType::Constant", EditConditionHides, Units="Degrees"))
	double EndAngleConstant = 360;

	/** End angle attribute, in degrees. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="End Angle", EditCondition="EndAngleInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector EndAngleAttribute;
};

namespace PCGExShapes
{
	class FCircle : public FShape
	{
	public:
		double Radius = 1;
		double StartAngle = 0;
		double EndAngle = TWO_PI;
		double AngleRange = TWO_PI;

		explicit FCircle(const PCGExData::FPointRef& InPointRef)
			: FShape(InPointRef)
		{
		}
	};
}

/**
 * 
 */
UCLASS(MinimalAPI)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExShapeCircleBuilder : public UPCGExShapeBuilderOperation
{
	GENERATED_BODY()

public:
	FPCGExShapeCircleConfig Config;

	virtual bool PrepareForSeeds(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InSeedDataFacade) override;
	virtual void PrepareShape(const PCGExData::FPointRef& Seed) override;
	virtual void BuildShape(TSharedPtr<PCGExShapes::FShape> InShape, TSharedPtr<PCGExData::FFacade> InDataFacade, TArrayView<FPCGPoint> PointView) override;

	virtual void Cleanup() override
	{
		StartAngleGetter.Reset();
		EndAngleGetter.Reset();
		Super::Cleanup();
	}

protected:
	TSharedPtr<PCGEx::TAttributeBroadcaster<double>> StartAngleGetter;
	TSharedPtr<PCGEx::TAttributeBroadcaster<double>> EndAngleGetter;
};


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExShapeCircleFactory : public UPCGExShapeBuilderFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExShapeCircleConfig Config;

	virtual UPCGExShapeBuilderOperation* CreateOperation(FPCGExContext* InContext) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Builder|Params")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExCreateShapeCircleSettings : public UPCGExShapeBuilderFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(ShapeBuilderCircle, "Shape : Circle", "Create points in a circular shape.", FName("Circle"))

#endif
	//~End UPCGSettings

	/** Shape properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExShapeCircleConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

protected:
	virtual bool IsCacheable() const override { return true; }
};
