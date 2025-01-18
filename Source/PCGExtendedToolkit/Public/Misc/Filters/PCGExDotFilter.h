// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "PCGExFilterFactoryProvider.h"
#include "UObject/Object.h"

#include "Data/PCGExPointFilter.h"
#include "PCGExPointsProcessor.h"


#include "PCGExDotFilter.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExDotFilterConfig
{
	GENERATED_BODY()

	FPCGExDotFilterConfig()
	{
	}

	/** Vector operand A */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(ShowOnlyInnerProperties))
	FPCGAttributePropertyInputSelector OperandA;

	/** Transform OperandA with the local point' transform */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bTransformOperandA = false;

	/** Type of OperandB */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType CompareAgainst = EPCGExInputValueType::Constant;

	/** Operand B for computing the dot product */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Operand B", EditCondition="CompareAgainst!=EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector OperandB;

	/** Operand B for computing the dot product. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Operand B", EditCondition="CompareAgainst==EPCGExInputValueType::Constant", EditConditionHides))
	FVector OperandBConstant = FVector::UpVector;

	/** Transform OperandB with the local point' transform */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bTransformOperandB = false;

	/** Dot comparison settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExDotComparisonDetails DotComparisonDetails;

	void Sanitize()
	{
	}
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExDotFilterFactory : public UPCGExFilterFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExDotFilterConfig Config;
	
	virtual bool Init(FPCGExContext* InContext) override;
	virtual TSharedPtr<PCGExPointFilter::FFilter> CreateFilter() const override;
	virtual bool RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const override;
};

namespace PCGExPointsFilter
{
	class /*PCGEXTENDEDTOOLKIT_API*/ FDotFilter final : public PCGExPointFilter::FSimpleFilter
	{
	public:
		explicit FDotFilter(const TObjectPtr<const UPCGExDotFilterFactory>& InFactory)
			: FSimpleFilter(InFactory), TypedFilterFactory(InFactory)
		{
			DotComparison = TypedFilterFactory->Config.DotComparisonDetails;
		}

		const TObjectPtr<const UPCGExDotFilterFactory> TypedFilterFactory;

		FPCGExDotComparisonDetails DotComparison;

		TSharedPtr<PCGExData::TBuffer<FVector>> OperandA;
		TSharedPtr<PCGExData::TBuffer<FVector>> OperandB;

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade) override;
		FORCEINLINE virtual bool Test(const int32 PointIndex) const override
		{
			const FPCGPoint& Point = PointDataFacade->Source->GetInPoint(PointIndex);
			const FVector B = OperandB ? OperandB->Read(PointIndex).GetSafeNormal() : TypedFilterFactory->Config.OperandBConstant;
			return DotComparison.Test(
				FVector::DotProduct(
					TypedFilterFactory->Config.bTransformOperandA ? Point.Transform.TransformVectorNoScale(OperandA->Read(PointIndex)) : OperandA->Read(PointIndex),
					TypedFilterFactory->Config.bTransformOperandB ? Point.Transform.TransformVectorNoScale(B) : B),
				PointIndex);
		}

		virtual ~FDotFilter() override
		{
		}
	};
}

///

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExDotFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		DotFilterFactory, "Filter : Dot", "Creates a filter definition that compares dot value of two vectors.",
		PCGEX_FACTORY_NAME_PRIORITY)
#endif
	//~End UPCGSettings

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExDotFilterConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

protected:
	virtual bool IsCacheable() const override { return true; }
};
