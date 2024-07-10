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

#include "PCGExBooleanCompareFilter.generated.h"


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExBooleanCompareFilterConfig
{
	GENERATED_BODY()

	FPCGExBooleanCompareFilterConfig()
	{
	}

	/** Operand A for testing -- Will be translated to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGAttributePropertyInputSelector OperandA;

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExEquality Comparison = EPCGExEquality::Equal;

	/** Type of OperandB */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExFetchType CompareAgainst = EPCGExFetchType::Constant;

	/** Operand B for testing -- Will be translated to `bool` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CompareAgainst==EPCGExFetchType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector OperandB;

	/** Operand B for testing */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CompareAgainst==EPCGExFetchType::Constant", EditConditionHides))
	bool OperandBConstant = true;
};


/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class PCGEXTENDEDTOOLKIT_API UPCGExBooleanCompareFilterFactory : public UPCGExFilterFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExBooleanCompareFilterConfig Config;

	virtual PCGExPointFilter::TFilter* CreateFilter() const override;
};

namespace PCGExPointsFilter
{
	class PCGEXTENDEDTOOLKIT_API TBooleanComparisonFilter final : public PCGExPointFilter::TFilter
	{
	public:
		explicit TBooleanComparisonFilter(const UPCGExBooleanCompareFilterFactory* InDefinition)
			: TFilter(InDefinition), TypedFilterFactory(InDefinition)
		{
		}

		const UPCGExBooleanCompareFilterFactory* TypedFilterFactory;

		PCGExData::FCache<bool>* OperandA = nullptr;
		PCGExData::FCache<bool>* OperandB = nullptr;

		virtual bool Init(const FPCGContext* InContext, PCGExData::FFacade* InPointDataFacade) override;
		FORCEINLINE virtual bool Test(const int32 PointIndex) const override
		{
			const double A = OperandA->Values[PointIndex];
			const double B = OperandB ? OperandB->Values[PointIndex] : TypedFilterFactory->Config.OperandBConstant;
			return TypedFilterFactory->Config.Comparison == EPCGExEquality::Equal ? A == B : A != B;
		}

		virtual ~TBooleanComparisonFilter() override
		{
			TypedFilterFactory = nullptr;
		}
	};
}

///

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class PCGEXTENDEDTOOLKIT_API UPCGExBooleanCompareFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		CompareFilterFactory, "Filter : Bool Compare", "Creates a filter definition that compares two boolean values.",
		PCGEX_FACTORY_NAME_PRIORITY)
#endif
	//~End UPCGSettings

public:
	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExBooleanCompareFilterConfig Config;

public:
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
