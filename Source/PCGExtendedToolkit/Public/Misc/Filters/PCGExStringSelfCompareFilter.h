// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "PCGExFilterFactoryProvider.h"
#include "UObject/Object.h"

#include "Data/PCGExPointFilter.h"
#include "PCGExPointsProcessor.h"


#include "PCGExStringSelfCompareFilter.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExStringSelfCompareFilterConfig
{
	GENERATED_BODY()

	FPCGExStringSelfCompareFilterConfig()
	{
	}

	/** Operand A for testing -- Will be translated to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName OperandA = NAME_None;

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExStringComparison Comparison = EPCGExStringComparison::StrictlyEqual;

	/** Index mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExIndexMode IndexMode = EPCGExIndexMode::Offset;

	/** Type of OperandB */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExFetchType CompareAgainst = EPCGExFetchType::Constant;

	/** Operand B for testing -- Will be translated to `int32` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CompareAgainst==EPCGExFetchType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector IndexAttribute;

	/** Operand B for testing */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CompareAgainst==EPCGExFetchType::Constant", EditConditionHides))
	int32 IndexConstant = -1;

	/** Index safety */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExIndexSafety IndexSafety = EPCGExIndexSafety::Clamp;
};


/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExStringSelfCompareFilterFactory : public UPCGExFilterFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExStringSelfCompareFilterConfig Config;

	virtual TSharedPtr<PCGExPointFilter::FFilter> CreateFilter() const override;
};

namespace PCGExPointsFilter
{
	class /*PCGEXTENDEDTOOLKIT_API*/ TStringSelfComparisonFilter final : public PCGExPointFilter::FSimpleFilter
	{
	public:
		explicit TStringSelfComparisonFilter(const TObjectPtr<const UPCGExStringSelfCompareFilterFactory>& InDefinition)
			: FSimpleFilter(InDefinition), TypedFilterFactory(InDefinition)
		{
		}

		const TObjectPtr<const UPCGExStringSelfCompareFilterFactory> TypedFilterFactory;

		TSharedPtr<PCGEx::TAttributeBroadcaster<FString>> OperandA;
		TSharedPtr<PCGExData::TBuffer<int32>> Index;
		bool bOffset = false;
		int32 MaxIndex = 0;

		virtual bool Init(const FPCGContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade) override;
		FORCEINLINE virtual bool Test(const int32 PointIndex) const override
		{
			const int32 IndexValue = Index ? Index->Read(PointIndex) : TypedFilterFactory->Config.IndexConstant;
			const int32 TargetIndex = PCGExMath::SanitizeIndex(bOffset ? PointIndex + IndexValue : IndexValue, MaxIndex, TypedFilterFactory->Config.IndexSafety);

			if (TargetIndex == -1) { return false; }

			const FString A = OperandA->SoftGet(PointIndex, PointDataFacade->Source->GetInPoint(PointIndex), TEXT(""));
			const FString B = OperandA->SoftGet(TargetIndex, PointDataFacade->Source->GetInPoint(TargetIndex), TEXT(""));
			return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B);
		}

		virtual ~TStringSelfComparisonFilter() override
		{
		}
	};
}

///

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExStringSelfCompareFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		CompareFilterFactory, "Filter : Self Compare (String)", "Creates a filter definition that compares an attribute value against itself at another index.",
		PCGEX_FACTORY_NAME_PRIORITY)
#endif
	//~End UPCGSettings

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExStringSelfCompareFilterConfig Config;

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
