// Copyright Timothé Lapetite 2024
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
};

namespace PCGExShapes
{
	class FCircle : public FShape
	{
	public:
		double Radius = 1;

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
	virtual void Cleanup() override
	{
		Super::Cleanup();
	}

	FPCGExShapeCircleConfig Config;

	virtual void PrepareShape(const PCGExData::FPointRef& Seed) override;
	virtual void BuildShape(TSharedPtr<PCGExShapes::FShape> InShape, TSharedPtr<PCGExData::FFacade> InDataFacade, TArrayView<FPCGPoint> PointView) override;
	
};


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExShapeCircleFactory : public UPCGExShapeBuilderFactoryBase
{
	GENERATED_BODY()

public:
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
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(ShapeBuilderCircle, "Shape Builder", "Create points in a circular shape.", FName("Circle"))

#endif
	//~End UPCGSettings

	/** Shape properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExShapeCircleConfig Config;

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;
};
