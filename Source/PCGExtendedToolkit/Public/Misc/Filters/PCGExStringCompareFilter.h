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
struct PCGEXTENDEDTOOLKIT_API FPCGExStringCompareFilterConfig
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
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class PCGEXTENDEDTOOLKIT_API UPCGExStringCompareFilterFactory : public UPCGExFilterFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExStringCompareFilterConfig Config;

	virtual PCGExPointFilter::TFilter* CreateFilter() const override;
};

namespace PCGExPointsFilter
{
	class PCGEXTENDEDTOOLKIT_API TStringCompareFilter final : public PCGExPointFilter::TFilter
	{
	public:
		explicit TStringCompareFilter(const UPCGExStringCompareFilterFactory* InFactory)
			: TFilter(InFactory), TypedFilterFactory(InFactory)
		{
		}

		const UPCGExStringCompareFilterFactory* TypedFilterFactory;

		PCGEx::FAttributeIOBase<FString>* OperandA = nullptr;
		PCGEx::FAttributeIOBase<FString>* OperandB = nullptr;

		virtual bool Init(const FPCGContext* InContext, PCGExData::FFacade* InPointDataFacade) override;
		FORCEINLINE virtual bool Test(const int32 PointIndex) const override
		{
			const FString A = OperandA->Values[PointIndex];
			const FString B = TypedFilterFactory->Config.CompareAgainst == EPCGExFetchType::Attribute ? OperandB->Values[PointIndex] : TypedFilterFactory->Config.OperandBConstant;

			switch (TypedFilterFactory->Config.Comparison)
			{
			case EPCGExStringComparison::StrictlyEqual:
				return A == B;
			case EPCGExStringComparison::StrictlyNotEqual:
				return A != B;
			case EPCGExStringComparison::LengthStrictlyEqual:
				return A.Len() == B.Len();
			case EPCGExStringComparison::LengthStrictlyUnequal:
				return A.Len() != B.Len();
			case EPCGExStringComparison::LengthEqualOrGreater:
				return A.Len() >= B.Len();
			case EPCGExStringComparison::LengthEqualOrSmaller:
				return A.Len() <= B.Len();
			case EPCGExStringComparison::StrictlyGreater:
				return A.Len() > B.Len();
			case EPCGExStringComparison::StrictlySmaller:
				return A.Len() < B.Len();
			case EPCGExStringComparison::LocaleStrictlyGreater:
				return A > B;
			case EPCGExStringComparison::LocaleStrictlySmaller:
				return A < B;
			case EPCGExStringComparison::Contains:
				return A.Contains(B);
			case EPCGExStringComparison::StartsWith:
				return A.StartsWith(B);
			case EPCGExStringComparison::EndsWith:
				return A.EndsWith(B);
			default:
				return false;
			}
		}

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
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		CompareFilterFactory, "Filter : String Compare", "Creates a filter definition that compares two attribute values.",
		PCGEX_FACTORY_NAME_PRIORITY)
#endif
	//~End UPCGSettings

public:
	/** State name.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExStringCompareFilterConfig Config;

public:
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
