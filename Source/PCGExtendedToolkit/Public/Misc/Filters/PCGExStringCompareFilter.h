// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "PCGExFilterFactoryProvider.h"
#include "UObject/Object.h"

#include "Data/PCGExDataFilter.h"
#include "PCGExPointsProcessor.h"

#include "PCGExStringCompareFilter.generated.h"


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExStringCompareFilterDescriptor
{
	GENERATED_BODY()

	FPCGExStringCompareFilterDescriptor()
	{
	}

	/** Operand A for testing -- Will be translated to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGAttributePropertyInputSelector OperandA;

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExStringComparison Comparison = EPCGExStringComparison::StrictlyEqual;

	/** Type of OperandB */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExFetchType CompareAgainst = EPCGExFetchType::Attribute;

	/** Operand B for testing -- Will be translated to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CompareAgainst==EPCGExFetchType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector OperandB;

	/** Operand B for testing -- Will be translated to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CompareAgainst==EPCGExFetchType::Constant", EditConditionHides))
	FString OperandBConstant = TEXT("PCGEx");
};


/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class PCGEXTENDEDTOOLKIT_API UPCGExStringCompareFilterFactory : public UPCGExFilterFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExStringCompareFilterDescriptor Descriptor;

	virtual PCGExDataFilter::TFilter* CreateFilter() const override;
};

namespace PCGExPointsFilter
{
	class PCGEXTENDEDTOOLKIT_API TStringCompareFilter final : public PCGExDataFilter::TFilter
	{
	public:
		explicit TStringCompareFilter(const UPCGExStringCompareFilterFactory* InFactory)
			: TFilter(InFactory), TypedFilterFactory(InFactory)
		{
		}

		const UPCGExStringCompareFilterFactory* TypedFilterFactory;

		PCGEx::TFAttributeReader<FString>* OperandA = nullptr;
		PCGEx::TFAttributeReader<FString>* OperandB = nullptr;

		virtual void Capture(const FPCGContext* InContext, PCGExDataCaching::FPool* InPrimaryDataCache) override;
		FORCEINLINE virtual bool Test(const int32 PointIndex) const override;

		virtual ~TStringCompareFilter() override
		{
			TypedFilterFactory = nullptr;
		}
	};
}

///

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class PCGEXTENDEDTOOLKIT_API UPCGExStringCompareFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		CompareFilterFactory, "Filter : String Compare", "Creates a filter definition that compares two attribute values.",
		PCGEX_FACTORY_NAME_PRIORITY)
#endif
	//~End UPCGSettings

public:
	/** State name.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExStringCompareFilterDescriptor Descriptor;

public:
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
