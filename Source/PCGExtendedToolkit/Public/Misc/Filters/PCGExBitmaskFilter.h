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
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBitmaskFilterConfig
{
	GENERATED_BODY()

	FPCGExBitmaskFilterConfig()
	{
	}

	/** Source value. (Operand A) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName FlagsAttribute = FName("Flags");

	/** Type of flag comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExBitflag64"))
	EPCGExBitflagComparison Comparison = EPCGExBitflagComparison::MatchPartial;

	/** Type of Mask */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExFetchType MaskType = EPCGExFetchType::Constant;

	/** Mask for testing -- Must be int64. (Operand B) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="MaskType==EPCGExFetchType::Attribute", EditConditionHides))
	FName BitmaskAttribute = FName("Mask");

	/** (Operand B) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="MaskType==EPCGExFetchType::Constant", EditConditionHides))
	int64 Bitmask = 0;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvertResult = false;
};


/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExBitmaskFilterFactory : public UPCGExFilterFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExBitmaskFilterConfig Config;

	virtual TSharedPtr<PCGExPointFilter::TFilter> CreateFilter() const override;
};

namespace PCGExPointsFilter
{
	class /*PCGEXTENDEDTOOLKIT_API*/ TBitmaskFilter final : public PCGExPointFilter::TFilter
	{
	public:
		explicit TBitmaskFilter(const TObjectPtr<const UPCGExBitmaskFilterFactory>& InDefinition)
			: TFilter(InDefinition), TypedFilterFactory(InDefinition), Bitmask(InDefinition->Config.Bitmask)
		{
		}

		TObjectPtr<const UPCGExBitmaskFilterFactory> TypedFilterFactory;

		TSharedPtr<PCGExData::TBuffer<int64>> FlagsReader;
		TSharedPtr<PCGExData::TBuffer<int64>> MaskReader;

		int64 Bitmask;

		virtual bool Init(const FPCGContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade) override;

		FORCEINLINE virtual bool Test(const int32 PointIndex) const override
		{
			const bool Result = PCGExCompare::Compare(
				TypedFilterFactory->Config.Comparison,
				FlagsReader->Read(PointIndex),
				MaskReader ? MaskReader->Read(PointIndex) : Bitmask);

			return TypedFilterFactory->Config.bInvertResult ? !Result : Result;
		}

		virtual ~TBitmaskFilter() override
		{
			TypedFilterFactory = nullptr;
		}
	};
}

///

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExBitmaskFilterProviderSettings : public UPCGExFilterProviderSettings
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
	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExBitmaskFilterConfig Config;

public:
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
