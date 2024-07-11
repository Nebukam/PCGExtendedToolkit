// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "PCGExCompare.h"
#include "PCGExFilterFactoryProvider.h"
#include "UObject/Object.h"

#include "Data/PCGExPointFilter.h"
#include "PCGExPointsProcessor.h"

#include "PCGExBoundsFilter.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExBoundsFilterConfig
{
	GENERATED_BODY()

	FPCGExBoundsFilterConfig()
	{
	}

	/** Transform OperandA with the local point' transform */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bCheckIfInside = true;
};

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class PCGEXTENDEDTOOLKIT_API UPCGExBoundsFilterFactory : public UPCGExFilterFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExBoundsFilterConfig Config;
	PCGExData::FFacade* BoundsDataFacade = nullptr;
	virtual bool Init(const FPCGContext* InContext) override;
	virtual PCGExPointFilter::TFilter* CreateFilter() const override;

	virtual void BeginDestroy() override;
};

namespace PCGExPointsFilter
{
	class PCGEXTENDEDTOOLKIT_API TBoundsFilter final : public PCGExPointFilter::TFilter
	{
	public:
		explicit TBoundsFilter(const UPCGExBoundsFilterFactory* InFactory)
			: TFilter(InFactory), TypedFilterFactory(InFactory)
		{
			Cloud = TypedFilterFactory->BoundsDataFacade ? TypedFilterFactory->BoundsDataFacade->GetCloud() : nullptr;
		}

		const UPCGExBoundsFilterFactory* TypedFilterFactory;

		PCGExGeo::FPointBoxCloud* Cloud = nullptr;

		virtual bool Init(const FPCGContext* InContext, PCGExData::FFacade* InPointDataFacade) override;
		virtual bool Test(const int32 PointIndex) const override;

		virtual ~TBoundsFilter() override
		{
			TypedFilterFactory = nullptr;
		}
	};
}

///

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class PCGEXTENDEDTOOLKIT_API UPCGExBoundsFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		BoundsFilterFactory, "Filter : Dot", "Creates a filter definition that compares dot value of two vectors.",
		PCGEX_FACTORY_NAME_PRIORITY)
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	//~End UPCGSettings

public:
	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExBoundsFilterConfig Config;

public:
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
