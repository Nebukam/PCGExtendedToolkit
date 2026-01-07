// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Utils/PCGExCompare.h"
#include "Core/PCGExFilterFactoryProvider.h"
#include "UObject/Object.h"

#include "Core/PCGExPointFilter.h"
#include "Data/Bitmasks/PCGExBitmaskDetails.h"

#include "PCGExBitmaskFilter.generated.h"


USTRUCT(BlueprintType)
struct PCGEXFILTERS_API FPCGExBitmaskFilterConfig
{
	GENERATED_BODY()

	FPCGExBitmaskFilterConfig()
	{
	}

	/** Source value. (Operand A) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName FlagsAttribute = FName("Flags");

	/** Type of flag comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExBitflagComparison Comparison = EPCGExBitflagComparison::MatchPartial;

	/** Type of Mask */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExInputValueType MaskInput = EPCGExInputValueType::Constant;

	/** Mask for testing -- Must be int64. (Operand B) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Bitmask (Attr)", EditCondition="MaskInput != EPCGExInputValueType::Constant", EditConditionHides))
	FName BitmaskAttribute = FName("Mask");

	/** (Operand B) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Bitmask", EditCondition="MaskInput == EPCGExInputValueType::Constant", EditConditionHides))
	int64 Bitmask = 0;

	/** External compositions applied to Operand B (whether it's a constant or not) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	TArray<FPCGExBitmaskRef> Compositions;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvertResult = false;

	PCGEX_SETTING_VALUE_DECL(Bitmask, int64)
};


/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class UPCGExBitmaskFilterFactory : public UPCGExPointFilterFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExBitmaskFilterConfig Config;

	virtual bool DomainCheck() override;

	virtual TSharedPtr<PCGExPointFilter::IFilter> CreateFilter() const override;
	virtual void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const override;
	virtual bool RegisterConsumableAttributes(FPCGExContext* InContext) const override;
};

namespace PCGExPointFilter
{
	class FBitmaskFilter final : public ISimpleFilter
	{
	public:
		explicit FBitmaskFilter(const TObjectPtr<const UPCGExBitmaskFilterFactory>& InDefinition)
			: ISimpleFilter(InDefinition), TypedFilterFactory(InDefinition), Bitmask(InDefinition->Config.Bitmask)
		{
		}

		TObjectPtr<const UPCGExBitmaskFilterFactory> TypedFilterFactory;

		TSharedPtr<PCGExData::TBuffer<int64>> FlagsReader;
		TSharedPtr<PCGExDetails::TSettingValue<int64>> MaskReader;

		int64 Bitmask;
		TArray<FPCGExSimpleBitmask> Compositions;

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade) override;
		virtual bool Test(const int32 PointIndex) const override;
		virtual bool Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const override;

		virtual ~FBitmaskFilter() override
		{
			TypedFilterFactory = nullptr;
		}
	};
}

///

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter", meta=(PCGExNodeLibraryDoc="filters/filters-points/bitmask"))
class UPCGExBitmaskFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(BitmaskFilterFactory, "Filter : Bitmask", "Filter using bitflag comparison.", PCGEX_FACTORY_NAME_PRIORITY)
#endif
	//~End UPCGSettings

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExBitmaskFilterConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
