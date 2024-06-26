// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "PCGExFilterFactoryProvider.h"
#include "UObject/Object.h"

#include "Data/PCGExDataFilter.h"
#include "PCGExPointsProcessor.h"

#include "PCGExBitflagFilter.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExBitflagFilterDescriptor
{
	GENERATED_BODY()

	FPCGExBitflagFilterDescriptor()
	{
	}

	/** Source value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName Value;

	/** Type of flag comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitflag64", DisplayName="Mask"))
	EPCGExBitflagComparison Comparison = EPCGExBitflagComparison::ContainsAll;

	/** Type of Mask */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExFetchType MaskType = EPCGExFetchType::Constant;

	/** Mask for testing -- Must be int64. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="MaskType==EPCGExFetchType::Attribute", DisplayName="Mask", EditConditionHides))
	FName MaskAttribute;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="MaskType==EPCGExFetchType::Constant", DisplayName="Mask", EditConditionHides))
	FPCGExCompositeBitflagValue Mask;
};


/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class PCGEXTENDEDTOOLKIT_API UPCGExBitflagFilterFactory : public UPCGExFilterFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExBitflagFilterDescriptor Descriptor;

	virtual PCGExDataFilter::TFilter* CreateFilter() const override;
};

namespace PCGExPointsFilter
{
	class PCGEXTENDEDTOOLKIT_API TBitflagFilter final : public PCGExDataFilter::TFilter
	{
	public:
		explicit TBitflagFilter(const UPCGExBitflagFilterFactory* InDefinition)
			: TFilter(InDefinition), TypedFilterFactory(InDefinition)
		{
		}

		const UPCGExBitflagFilterFactory* TypedFilterFactory;

		PCGExDataCaching::FCache<int64>* ValueCache = nullptr;
		PCGExDataCaching::FCache<int64>* MaskCache = nullptr;

		int64 CompositeMask;

		virtual void Capture(const FPCGContext* InContext, PCGExDataCaching::FPool* InPrimaryDataCache) override;
		FORCEINLINE virtual bool Test(const int32 PointIndex) const override;

		virtual ~TBitflagFilter() override
		{
			TypedFilterFactory = nullptr;
		}
	};
}

///

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class PCGEXTENDEDTOOLKIT_API UPCGExBitflagFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		CompareFilterFactory, "Filter : Bitflag", "Filter using bitflag comparison.",
		PCGEX_FACTORY_NAME_PRIORITY)
#endif
	//~End UPCGSettings

public:
	/** Filter Descriptor.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExBitflagFilterDescriptor Descriptor;

public:
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
