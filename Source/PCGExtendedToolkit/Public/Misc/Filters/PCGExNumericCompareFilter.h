// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "PCGExFilterFactoryProvider.h"
#include "UObject/Object.h"

#include "Data/PCGExDataFilter.h"
#include "PCGExPointsProcessor.h"

#include "PCGExNumericCompareFilter.generated.h"


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExNumericCompareFilterDescriptor
{
	GENERATED_BODY()

	FPCGExNumericCompareFilterDescriptor()
	{
	}

	/** Operand A for testing -- Will be broadcasted to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGAttributePropertyInputSelector OperandA;

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExComparison Comparison = EPCGExComparison::NearlyEqual;

	/** Type of OperandB */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExOperandType CompareAgainst = EPCGExOperandType::Attribute;

	/** Operand B for testing -- Will be broadcasted to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CompareAgainst==EPCGExOperandType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector OperandB;

	/** Operand B for testing -- Will be broadcasted to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CompareAgainst==EPCGExOperandType::Constant", EditConditionHides))
	double OperandBConstant = 0;

	/** Rounding mode for relative measures */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Comparison==EPCGExComparison::NearlyEqual || Comparison==EPCGExComparison::NearlyNotEqual", EditConditionHides))
	double Tolerance = 0.001;
};


/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class PCGEXTENDEDTOOLKIT_API UPCGExNumericCompareFilterFactory : public UPCGExFilterFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExInputDescriptor OperandA;
	EPCGExComparison Comparison = EPCGExComparison::NearlyEqual;
	EPCGExOperandType CompareAgainst = EPCGExOperandType::Attribute;
	FPCGExInputDescriptor OperandB;
	double OperandBConstant;
	double Tolerance = 0.001;

	void ApplyDescriptor(const FPCGExNumericCompareFilterDescriptor& Descriptor)
	{
		OperandA = FPCGExInputDescriptor(Descriptor.OperandA);
		Comparison = Descriptor.Comparison;
		CompareAgainst = Descriptor.CompareAgainst;
		OperandB = FPCGExInputDescriptor(Descriptor.OperandB);
		OperandBConstant = Descriptor.OperandBConstant;
		Tolerance = Descriptor.Tolerance;
	}

	virtual PCGExDataFilter::TFilter* CreateFilter() const override;
};

namespace PCGExPointsFilter
{
	class PCGEXTENDEDTOOLKIT_API TNumericComparisonFilter : public PCGExDataFilter::TFilter
	{
	public:
		explicit TNumericComparisonFilter(const UPCGExNumericCompareFilterFactory* InDefinition)
			: TFilter(InDefinition), TypedFilterFactory(InDefinition)
		{
		}

		const UPCGExNumericCompareFilterFactory* TypedFilterFactory;

		PCGEx::FLocalSingleFieldGetter* OperandA = nullptr;
		PCGEx::FLocalSingleFieldGetter* OperandB = nullptr;

		virtual void Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO) override;
		FORCEINLINE virtual bool Test(const int32 PointIndex) const override;

		virtual ~TNumericComparisonFilter() override
		{
			TypedFilterFactory = nullptr;
			PCGEX_DELETE(OperandA)
			PCGEX_DELETE(OperandB)
		}
	};
}

///

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class PCGEXTENDEDTOOLKIT_API UPCGExNumericCompareFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		CompareFilterDefinition, "Filter : Numeric Compare", "Creates a filter definition that compares two attribute values.",
		FName(GetDisplayName()))
#endif
	//~End UPCGSettings

public:
	/** Filter Descriptor.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExNumericCompareFilterDescriptor Descriptor;

public:
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
