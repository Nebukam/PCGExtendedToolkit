// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "PCGExFilterFactoryProvider.h"
#include "UObject/Object.h"

#include "Data/PCGExPointFilter.h"
#include "PCGExPointsProcessor.h"

#include "PCGExBitmaskFilter.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExBitmaskFilterDescriptor
{
	GENERATED_BODY()

	FPCGExBitmaskFilterDescriptor()
	{
	}

	/** Source value. (Operand A) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName Value = FName("Flags");

	/** Type of flag comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitflag64", DisplayName="Mask"))
	EPCGExBitflagComparison Comparison = EPCGExBitflagComparison::ContainsAll;

	/** Type of Mask */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExFetchType MaskType = EPCGExFetchType::Constant;

	/** Mask for testing -- Must be int64. (Operand B) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="MaskType==EPCGExFetchType::Attribute", DisplayName="Mask", EditConditionHides))
	FName MaskAttribute = FName("Mask");

	/** (Operand B) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="MaskType==EPCGExFetchType::Constant", DisplayName="Mask", EditConditionHides))
	FPCGExBitmask BitMask;
};


/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class PCGEXTENDEDTOOLKIT_API UPCGExBitmaskFilterFactory : public UPCGExFilterFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExBitmaskFilterDescriptor Descriptor;

	virtual PCGExPointFilter::TFilter* CreateFilter() const override;
};

namespace PCGExPointsFilter
{
	class PCGEXTENDEDTOOLKIT_API TBitmaskFilter final : public PCGExPointFilter::TFilter
	{
	public:
		explicit TBitmaskFilter(const UPCGExBitmaskFilterFactory* InDefinition)
			: TFilter(InDefinition), TypedFilterFactory(InDefinition), CompositeMask(InDefinition->Descriptor.BitMask.Get())
		{
		}

		const UPCGExBitmaskFilterFactory* TypedFilterFactory;

		PCGEx::FAttributeIOBase<int64>* ValueReader = nullptr;
		PCGEx::FAttributeIOBase<int64>* MaskReader = nullptr;

		int64 CompositeMask;

		virtual bool Init(const FPCGContext* InContext, PCGExData::FFacade* InPointDataFacade) override;

		virtual bool Test(const int32 PointIndex) const override;

		virtual ~TBitmaskFilter() override
		{
			TypedFilterFactory = nullptr;
		}
	};
}

///

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class PCGEXTENDEDTOOLKIT_API UPCGExBitmaskFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		CompareFilterFactory, "Filter : Bitmask", "Filter using bitflag comparison.",
		PCGEX_FACTORY_NAME_PRIORITY)
#endif
	//~End UPCGSettings

public:
	/** Filter Descriptor.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExBitmaskFilterDescriptor Descriptor;

public:
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
