// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"


#include "Graph/PCGExCluster.h"
#include "Graph/Filters/PCGExClusterFilter.h"
#include "Misc/Filters/PCGExFilterFactoryProvider.h"

#include "PCGExNeighborsCountFilter.generated.h"


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExNeighborsCountFilterConfig
{
	GENERATED_BODY()

	FPCGExNeighborsCountFilterConfig()
	{
	}

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExComparison Comparison = EPCGExComparison::NearlyEqual;

	/** Type of Count */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExFetchType CompareAgainst = EPCGExFetchType::Constant;

	/** Operand A for testing -- Will be translated to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="CompareAgainst==EPCGExFetchType::Attribute", EditConditionHides, ShowOnlyInnerProperties, DisplayName="Operand A (First)"))
	FPCGAttributePropertyInputSelector LocalCount;

	/** Constant Operand A for testing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CompareAgainst==EPCGExFetchType::Constant", EditConditionHides))
	int32 Count = 0;

	/** Rounding mode for near measures */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Comparison==EPCGExComparison::NearlyEqual || Comparison==EPCGExComparison::NearlyNotEqual", EditConditionHides))
	double Tolerance = 0.001;
};

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExNeighborsCountFilterFactory : public UPCGExClusterFilterFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExNeighborsCountFilterConfig Config;

	virtual PCGExPointFilter::TFilter* CreateFilter() const override;
};

namespace PCGExNodeNeighborsCount
{
	class PCGEXTENDEDTOOLKIT_API FNeighborsCountFilter final : public PCGExClusterFilter::TFilter
	{
	public:
		explicit FNeighborsCountFilter(const UPCGExNeighborsCountFilterFactory* InFactory)
			: TFilter(InFactory), TypedFilterFactory(InFactory)
		{
		}

		const UPCGExNeighborsCountFilterFactory* TypedFilterFactory;

		PCGExData::FCache<double>* LocalCount = nullptr;

		virtual bool Init(const FPCGContext* InContext, PCGExCluster::FCluster* InCluster, PCGExData::FFacade* InPointDataFacade, PCGExData::FFacade* InEdgeDataFacade) override;
		virtual bool Test(const PCGExCluster::FNode& Node) const override;

		virtual ~FNeighborsCountFilter() override
		{
			TypedFilterFactory = nullptr;
		}
	};
}


/** Outputs a single GraphParam to be consumed by other nodes */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExNeighborsCountFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		NodeNeighborsCountFilterFactory, "Cluster Filter : Neighbors Count", "Check against the node' neighbor count.",
		PCGEX_FACTORY_NAME_PRIORITY)
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorClusterFilter; }
#endif
	//~End UPCGSettings

public:
	/** Test Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExNeighborsCountFilterConfig Config;

public:
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
