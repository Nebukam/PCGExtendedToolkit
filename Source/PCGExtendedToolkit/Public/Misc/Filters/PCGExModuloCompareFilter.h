// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"

#include "PCGExFilterFactoryProvider.h"
#include "UObject/Object.h"

#include "Data/PCGExPointFilter.h"
#include "PCGExPointsProcessor.h"


#include "PCGExModuloCompareFilter.generated.h"


USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExModuloCompareFilterConfig
{
	GENERATED_BODY()

	FPCGExModuloCompareFilterConfig()
	{
	}

	/** Operand A for testing -- Will be translated to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGAttributePropertyInputSelector OperandA;

	/** Type of OperandB */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType OperandBSource = EPCGExInputValueType::Constant;

	/** Operand B for testing (Modulo base) -- Will be translated to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Operand B", EditCondition="OperandBSource!=EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector OperandB;

	/** Operand B for testing */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Operand B", EditCondition="OperandBSource==EPCGExInputValueType::Constant", EditConditionHides))
	double OperandBConstant = 2;

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExComparison Comparison = EPCGExComparison::NearlyEqual;

	/** Type of OperandC */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType CompareAgainst = EPCGExInputValueType::Constant;

	/** Operand C for testing -- Will be translated to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CompareAgainst!=EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector OperandC;

	/** Operand C for testing */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Operand C", EditCondition="CompareAgainst==EPCGExInputValueType::Constant", EditConditionHides))
	double OperandCConstant = 0;

	/** Rounding mode for relative measures */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Operand C", EditCondition="Comparison==EPCGExComparison::NearlyEqual || Comparison==EPCGExComparison::NearlyNotEqual", EditConditionHides))
	double Tolerance = DBL_COMPARE_TOLERANCE;

	/** Which value to return when dealing with zero-values */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool ZeroResult = true;
};


/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExModuloCompareFilterFactory : public UPCGExFilterFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExModuloCompareFilterConfig Config;

	virtual TSharedPtr<PCGExPointFilter::FFilter> CreateFilter() const override;
	virtual bool RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const override;
};

namespace PCGExPointsFilter
{
	class /*PCGEXTENDEDTOOLKIT_API*/ TModuloComparisonFilter final : public PCGExPointFilter::FSimpleFilter
	{
	public:
		explicit TModuloComparisonFilter(const TObjectPtr<const UPCGExModuloCompareFilterFactory>& InDefinition)
			: FSimpleFilter(InDefinition), TypedFilterFactory(InDefinition)
		{
		}

		const TObjectPtr<const UPCGExModuloCompareFilterFactory> TypedFilterFactory;

		TSharedPtr<PCGExData::TBuffer<double>> OperandA;
		TSharedPtr<PCGExData::TBuffer<double>> OperandB;
		TSharedPtr<PCGExData::TBuffer<double>> OperandC;

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade) override;
		FORCEINLINE virtual bool Test(const int32 PointIndex) const override
		{
			const double A = OperandA->Read(PointIndex);
			const double B = OperandB ? OperandB->Read(PointIndex) : TypedFilterFactory->Config.OperandBConstant;
			const double C = OperandC ? OperandC->Read(PointIndex) : TypedFilterFactory->Config.OperandCConstant;
			if (A == 0 || B == 0) { return TypedFilterFactory->Config.ZeroResult; }
			return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, FMath::Fmod(A, B), C, TypedFilterFactory->Config.Tolerance);
		}

		virtual ~TModuloComparisonFilter() override
		{
		}
	};
}

///

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExModuloCompareFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		ModuloCompareFilterFactory, "Filter : Modulo Compare", "A % B != C",
		PCGEX_FACTORY_NAME_PRIORITY)
#endif
	//~End UPCGSettings

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExModuloCompareFilterConfig Config;

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
