// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExShape.h"
#include "Core/PCGExShapeBuilderFactoryProvider.h"
#include "Core/PCGExShapeBuilderOperation.h"

#include "PCGExShapeCircle.generated.h"

USTRUCT(BlueprintType)
struct FPCGExShapeCircleConfig : public FPCGExShapeConfigBase
{
	GENERATED_BODY()

	FPCGExShapeCircleConfig()
		: FPCGExShapeConfigBase()
	{
	}

	/** Start angle source. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExInputValueType StartAngleInput = EPCGExInputValueType::Constant;

	/** Start angle attribute, in degrees. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Start Angle (Attr)", EditCondition="StartAngleInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector StartAngleAttribute;

	/** Start angle constant, in degrees. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Start Angle", EditCondition="StartAngleInput == EPCGExInputValueType::Constant", EditConditionHides, Units="Degrees"))
	double StartAngleConstant = 0;

	PCGEX_SETTING_VALUE_DECL(StartAngle, double)

	/** End angle source. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExInputValueType EndAngleInput = EPCGExInputValueType::Constant;

	/** End angle attribute, in degrees. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="End Angle (Attr)", EditCondition="EndAngleInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector EndAngleAttribute;

	/** End angle constant, in degrees. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="End Angle", EditCondition="EndAngleInput == EPCGExInputValueType::Constant", EditConditionHides, Units="Degrees"))
	double EndAngleConstant = 360;

	PCGEX_SETTING_VALUE_DECL(EndAngle, double)

	/** If enabled, will flag circle as being closed if possible. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bIsClosedLoop = true;
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
		bool bClosedLoop = false;

		explicit FCircle(const PCGExData::FConstPoint& InPointRef)
			: FShape(InPointRef)
		{
		}
	};
}

/**
 * 
 */
class FPCGExShapeCircleBuilder : public FPCGExShapeBuilderOperation
{
public:
	FPCGExShapeCircleConfig Config;

	virtual bool PrepareForSeeds(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InSeedDataFacade) override;
	virtual void PrepareShape(const PCGExData::FConstPoint& Seed) override;
	virtual void BuildShape(TSharedPtr<PCGExShapes::FShape> InShape, TSharedPtr<PCGExData::FFacade> InDataFacade, const PCGExData::FScope& Scope, bool bOwnsData = false) override;

protected:
	TSharedPtr<PCGExDetails::TSettingValue<double>> StartAngle;
	TSharedPtr<PCGExDetails::TSettingValue<double>> EndAngle;
};


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExShapeCircleFactory : public UPCGExShapeBuilderFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExShapeCircleConfig Config;

	virtual TSharedPtr<FPCGExShapeBuilderOperation> CreateOperation(FPCGExContext* InContext) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Builder|Params", meta=(PCGExNodeLibraryDoc="misc/shapes/shape-circle"))
class UPCGExCreateShapeCircleSettings : public UPCGExShapeBuilderFactoryProviderSettings
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
