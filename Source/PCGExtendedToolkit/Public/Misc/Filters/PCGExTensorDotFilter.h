// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "PCGExFilterFactoryProvider.h"
#include "UObject/Object.h"

#include "Data/PCGExPointFilter.h"
#include "PCGExPointsProcessor.h"
#include "Transform/Tensors/PCGExTensor.h"
#include "Transform/Tensors/PCGExTensorHandler.h"


#include "PCGExTensorDotFilter.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExTensorDotFilterConfig
{
	GENERATED_BODY()

	FPCGExTensorDotFilterConfig()
	{
	}

	/** Vector operand A */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(ShowOnlyInnerProperties))
	FPCGAttributePropertyInputSelector OperandA;

	/** Transform OperandA with the local point' transform */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bTransformOperandA = false;

	/** Dot comparison settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExDotComparisonDetails DotComparisonDetails;

	/** Tensor sampling settings. Note that these are applied on the flattened sample, e.g after & on top of individual tensors' mutations. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Tensor Sampling Settings"))
	FPCGExTensorHandlerDetails TensorHandlerDetails;
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExTensorDotFilterFactory : public UPCGExFilterFactoryData
{
	GENERATED_BODY()

public:
	FPCGExTensorDotFilterConfig Config;
	TSharedPtr<PCGExTensor::FTensorsHandler> TensorsHandler;

	virtual bool Init(FPCGExContext* InContext) override;
	virtual TSharedPtr<PCGExPointFilter::FFilter> CreateFilter() const override;
	virtual bool RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const override;

	TArray<TObjectPtr<const UPCGExTensorFactoryData>> TensorFactories;
};

namespace PCGExPointsFilter
{
	class /*PCGEXTENDEDTOOLKIT_API*/ TTensorDotFilter final : public PCGExPointFilter::FSimpleFilter
	{
	public:
		explicit TTensorDotFilter(const TObjectPtr<const UPCGExTensorDotFilterFactory>& InFactory)
			: FSimpleFilter(InFactory), TypedFilterFactory(InFactory)
		{
			DotComparison = TypedFilterFactory->Config.DotComparisonDetails;
			TensorsHandler = TypedFilterFactory->TensorsHandler;
		}

		const TObjectPtr<const UPCGExTensorDotFilterFactory> TypedFilterFactory;

		FPCGExDotComparisonDetails DotComparison;
		TSharedPtr<PCGExTensor::FTensorsHandler> TensorsHandler;

		TSharedPtr<PCGExData::TBuffer<FVector>> OperandA;

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade) override;
		FORCEINLINE virtual bool Test(const int32 PointIndex) const override
		{
			const FPCGPoint& Point = PointDataFacade->Source->GetInPoint(PointIndex);

			bool bSuccess = false;
			const PCGExTensor::FTensorSample Sample = TensorsHandler->Sample(Point.Transform, bSuccess);

			if (!bSuccess) { return false; }

			return DotComparison.Test(
				FVector::DotProduct(
					TypedFilterFactory->Config.bTransformOperandA ? OperandA->Read(PointIndex) : Point.Transform.TransformVectorNoScale(OperandA->Read(PointIndex)),
					Sample.DirectionAndSize.GetSafeNormal()),
				DotComparison.GetComparisonThreshold(PointIndex));
		}

		virtual ~TTensorDotFilter() override
		{
		}
	};
}

///

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExTensorDotFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		TensorDotFilterFactory, "Filter : Tensor Dot", "Creates a filter definition that compares dot value of a vector and tensors.",
		PCGEX_FACTORY_NAME_PRIORITY)
#endif
	//~End UPCGSettings

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

public:
	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExTensorDotFilterConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};

inline TArray<FPCGPinProperties> UPCGExTensorDotFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExTensor::SourceTensorsLabel, "Tensors", Required, {})
	return PinProperties;
}
