// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "PCGExFilterFactoryProvider.h"
#include "UObject/Object.h"

#include "Data/PCGExPointFilter.h"
#include "PCGExPointsProcessor.h"
#include "PCGExRandom.h"


#include "PCGExRandomFilter.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExRandomFilterConfig
{
	GENERATED_BODY()

	FPCGExRandomFilterConfig()
	{
	}

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	int32 RandomSeed = 0;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvertResult = false;
};


/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExRandomFilterFactory : public UPCGExFilterFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExRandomFilterConfig Config;

	virtual TSharedPtr<PCGExPointFilter::TFilter> CreateFilter() const override;
};

namespace PCGExPointsFilter
{
	class /*PCGEXTENDEDTOOLKIT_API*/ TRandomFilter final : public PCGExPointFilter::TFilter
	{
	public:
		explicit TRandomFilter(const TObjectPtr<const UPCGExRandomFilterFactory>& InDefinition)
			: TFilter(InDefinition), TypedFilterFactory(InDefinition), RandomSeed(InDefinition->Config.RandomSeed)
		{
		}

		const TObjectPtr<const UPCGExRandomFilterFactory> TypedFilterFactory;

		int32 RandomSeed;

		virtual bool Init(const FPCGContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade) override;

		FORCEINLINE virtual bool Test(const int32 PointIndex) const override
		{
			const int32 RandomValue = FRandomStream(PCGExRandom::GetRandomStreamFromPoint(PointDataFacade->Source->GetInPoint(PointIndex), RandomSeed)).RandRange(0, 100);
			return TypedFilterFactory->Config.bInvertResult ? RandomValue <= 50 : RandomValue >= 50;
		}

		virtual ~TRandomFilter() override
		{
		}
	};
}

///

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExRandomFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(CompareFilterFactory, "Filter : Random", "Filter using a random value.")
#endif
	//~End UPCGSettings

public:
	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExRandomFilterConfig Config;

public:
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;
};
