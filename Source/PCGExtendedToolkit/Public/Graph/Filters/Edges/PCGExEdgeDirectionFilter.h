// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/Filters/PCGExAdjacency.h"
#include "PCGExDetails.h"


#include "Graph/PCGExCluster.h"
#include "Graph/Filters/PCGExClusterFilter.h"
#include "Misc/Filters/PCGExFilterFactoryProvider.h"

#include "PCGExEdgeDirectionFilter.generated.h"


USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExIsoEdgeDirectionFilterConfig
{
	GENERATED_BODY()

	FPCGExIsoEdgeDirectionFilterConfig()
	{
	}

	/** Type of check; Note that Fast comparison ignores adjacency consolidation. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExDirectionCheckMode ComparisonQuality = EPCGExDirectionCheckMode::Dot;

	/** Adjacency Settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExAdjacencySettings Adjacency;

	/** Direction orientation */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExAdjacencyDirectionOrigin DirectionOrder = EPCGExAdjacencyDirectionOrigin::FromNode;

	/** Where to read the compared direction from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExFetchType CompareAgainst = EPCGExFetchType::Constant;

	/** Operand B for testing -- Will be translated to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CompareAgainst==EPCGExFetchType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector Direction;

	/** Direction for computing the dot product against the edge's. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CompareAgainst==EPCGExFetchType::Constant", EditConditionHides))
	FVector DirectionConstant = FVector::UpVector;

	/** Transform the reference direction with the local point' transform */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bTransformDirection = false;

	/** Dot comparison settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="ComparisonQuality==EPCGExDirectionCheckMode::Dot", EditConditionHides))
	FPCGExDotComparisonDetails DotComparisonDetails;

	/** Hash comparison settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="ComparisonQuality==EPCGExDirectionCheckMode::Hash", EditConditionHides))
	FPCGExVectorHashComparisonDetails HashComparisonDetails;
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExIsoEdgeDirectionFilterFactory : public UPCGExEdgeFilterFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExIsoEdgeDirectionFilterConfig Config;

	virtual TSharedPtr<PCGExPointFilter::TFilter> CreateFilter() const override;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FIsoEdgeDirectionFilter final : public PCGExClusterFilter::TFilter
{
public:
	explicit FIsoEdgeDirectionFilter(const UPCGExIsoEdgeDirectionFilterFactory* InFactory)
		: TFilter(InFactory), TypedFilterFactory(InFactory)
	{
		Adjacency = InFactory->Config.Adjacency;
		DotComparison = InFactory->Config.DotComparisonDetails;
		HashComparison = InFactory->Config.HashComparisonDetails;
	}

	const UPCGExIsoEdgeDirectionFilterFactory* TypedFilterFactory;

	bool bFromNode = true;
	bool bUseDot = true;

	TArray<double> CachedThreshold;
	FPCGExAdjacencySettings Adjacency;
	FPCGExDotComparisonDetails DotComparison;
	FPCGExVectorHashComparisonDetails HashComparison;

	TSharedPtr<PCGExData::TBuffer<FVector>> OperandDirection;

	virtual bool Init(const FPCGContext* InContext, const TSharedPtr<PCGExCluster::FCluster>& InCluster, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade, const TSharedPtr<PCGExData::FFacade>& InEdgeDataFacade) override;
	virtual bool Test(const PCGExCluster::FNode& Node) const override;

	bool TestDot(const PCGExCluster::FNode& Node) const;
	bool TestHash(const PCGExCluster::FNode& Node) const;

	virtual ~FIsoEdgeDirectionFilter() override
	{
		TypedFilterFactory = nullptr;
	}
};


UCLASS(Abstract, MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExIsoEdgeDirectionFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		IsoEdgeDirectionFilterFactory, "Cluster Filter : Edge Direction (Edge)", "Dot product comparison of the edge direction against a local attribute or constant.",
		PCGEX_FACTORY_NAME_PRIORITY)
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorClusterFilter; }
#endif

	/** Test Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExIsoEdgeDirectionFilterConfig Config;

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
