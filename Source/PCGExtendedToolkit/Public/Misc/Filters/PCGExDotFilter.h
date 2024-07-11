// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "PCGExCompare.h"
#include "PCGExFilterFactoryProvider.h"
#include "UObject/Object.h"

#include "Data/PCGExPointFilter.h"
#include "PCGExPointsProcessor.h"

#include "PCGExDotFilter.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExDotFilterConfig
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
	EPCGExFetchType CompareAgainst = EPCGExFetchType::Constant;

	/** Operand B for computing the dot product */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CompareAgainst==EPCGExFetchType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector OperandB;

	/** Operand B for computing the dot product. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CompareAgainst==EPCGExFetchType::Constant", EditConditionHides))
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
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class PCGEXTENDEDTOOLKIT_API UPCGExDotFilterFactory : public UPCGExFilterFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExDotFilterConfig Config;
	virtual bool Init(const FPCGContext* InContext) override;
	virtual PCGExPointFilter::TFilter* CreateFilter() const override;
};

namespace PCGExPointsFilter
{
	class PCGEXTENDEDTOOLKIT_API TDotFilter final : public PCGExPointFilter::TFilter
	{
	public:
		explicit TDotFilter(const UPCGExDotFilterFactory* InFactory)
			: TFilter(InFactory), TypedFilterFactory(InFactory)
		{
			DotComparison = TypedFilterFactory->Config.DotComparisonDetails;
		}

		const UPCGExDotFilterFactory* TypedFilterFactory;

		FPCGExDotComparisonDetails DotComparison;

		PCGExData::FCache<FVector>* OperandA = nullptr;
		PCGExData::FCache<FVector>* OperandB = nullptr;

		virtual bool Init(const FPCGContext* InContext, PCGExData::FFacade* InPointDataFacade) override;
		FORCEINLINE virtual bool Test(const int32 PointIndex) const override
		{
			const FPCGPoint& Point = PointDataFacade->Source->GetInPoint(PointIndex);

			const FVector A = TypedFilterFactory->Config.bTransformOperandA ?
				                  OperandA->Values[PointIndex] :
				                  Point.Transform.TransformVectorNoScale(OperandA->Values[PointIndex]);

			FVector B = OperandB ? OperandB->Values[PointIndex].GetSafeNormal() : TypedFilterFactory->Config.OperandBConstant;
			if (TypedFilterFactory->Config.bTransformOperandB) { B = Point.Transform.TransformVectorNoScale(B); }

			const double Dot = DotComparison.bUnsignedDot ? FMath::Abs(FVector::DotProduct(A, B)) : FVector::DotProduct(A, B);
			return DotComparison.Test(Dot, DotComparison.GetDot(PointIndex));
		}

		virtual ~TDotFilter() override
		{
			TypedFilterFactory = nullptr;
		}
	};
}

///

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class PCGEXTENDEDTOOLKIT_API UPCGExDotFilterProviderSettings : public UPCGExFilterProviderSettings
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

public:
	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExDotFilterConfig Config;

public:
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
