// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "PCGExFilterFactoryProvider.h"
#include "UObject/Object.h"

#include "Data/PCGExPointFilter.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGExAttributeHelpers.h"


#include "PCGExStringCompareFilter.generated.h"


USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExStringCompareFilterConfig
{
	GENERATED_BODY()

	FPCGExStringCompareFilterConfig()
	{
	}

	/** Operand A for testing -- Will be translated to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName OperandA = NAME_None;

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExStringComparison Comparison = EPCGExStringComparison::StrictlyEqual;

	/** Type of OperandB */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExFetchType CompareAgainst = EPCGExFetchType::Constant;

	/** Operand B for testing -- Will be translated to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CompareAgainst==EPCGExFetchType::Attribute", EditConditionHides))
	FName OperandB = NAME_None;

	/** Operand B for testing */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CompareAgainst==EPCGExFetchType::Constant", EditConditionHides))
	FString OperandBConstant = TEXT("MyString");
};


/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExStringCompareFilterFactory : public UPCGExFilterFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExStringCompareFilterConfig Config;

	virtual TSharedPtr<PCGExPointFilter::FFilter> CreateFilter() const override;
};

namespace PCGExPointsFilter
{
	class /*PCGEXTENDEDTOOLKIT_API*/ TStringCompareFilter final : public PCGExPointFilter::FSimpleFilter
	{
	public:
		explicit TStringCompareFilter(const TObjectPtr<const UPCGExStringCompareFilterFactory>& InFactory)
			: FSimpleFilter(InFactory), TypedFilterFactory(InFactory)
		{
		}

		const TObjectPtr<const UPCGExStringCompareFilterFactory> TypedFilterFactory;

		TSharedPtr<PCGEx::TAttributeBroadcaster<FString>> OperandA;
		TSharedPtr<PCGEx::TAttributeBroadcaster<FString>> OperandB;

		virtual bool Init(const FPCGContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade) override;
		FORCEINLINE virtual bool Test(const int32 PointIndex) const override
		{
			const FPCGPoint& Point = PointDataFacade->Source->GetInPoint(PointIndex);
			const FString A = OperandA->SoftGet(PointIndex, Point, TEXT(""));
			const FString B = TypedFilterFactory->Config.CompareAgainst == EPCGExFetchType::Attribute ? OperandB->SoftGet(PointIndex, Point, TEXT("")) : TypedFilterFactory->Config.OperandBConstant;
			return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B);
		}

		virtual ~TStringCompareFilter() override
		{
		}
	};
}

///

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExStringCompareFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		CompareFilterFactory, "Filter : Compare (String)", "Creates a filter definition that compares two string attribute values.",
		PCGEX_FACTORY_NAME_PRIORITY)
#endif
	//~End UPCGSettings

	/** State name.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExStringCompareFilterConfig Config;

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
