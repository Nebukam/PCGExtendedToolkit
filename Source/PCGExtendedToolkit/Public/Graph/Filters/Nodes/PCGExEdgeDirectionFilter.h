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
struct PCGEXTENDEDTOOLKIT_API FPCGExEdgeDirectionFilterConfig
{
	GENERATED_BODY()

	FPCGExEdgeDirectionFilterConfig()
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
	EPCGExFetchType CompareAgainst = EPCGExFetchType::Attribute;

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
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExEdgeDirectionFilterFactory : public UPCGExClusterFilterFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExEdgeDirectionFilterConfig Config;

	virtual PCGExPointFilter::TFilter* CreateFilter() const override;
};

namespace PCGExNodeAdjacency
{
	class PCGEXTENDEDTOOLKIT_API FEdgeDirectionFilter final : public PCGExClusterFilter::TFilter
	{
	public:
		explicit FEdgeDirectionFilter(const UPCGExEdgeDirectionFilterFactory* InFactory)
			: TFilter(InFactory), TypedFilterFactory(InFactory)
		{
			Adjacency = InFactory->Config.Adjacency;
			DotComparison = InFactory->Config.DotComparisonDetails;
			HashComparison = InFactory->Config.HashComparisonDetails;
		}

		const UPCGExEdgeDirectionFilterFactory* TypedFilterFactory;

		bool bFromNode = true;
		bool bUseDot = true;

		TArray<double> CachedThreshold;
		FPCGExAdjacencySettings Adjacency;
		FPCGExDotComparisonDetails DotComparison;
		FPCGExVectorHashComparisonDetails HashComparison;

		PCGExData::FCache<FVector>* OperandDirection = nullptr;

		virtual bool Init(const FPCGContext* InContext, PCGExCluster::FCluster* InCluster, PCGExData::FFacade* InPointDataFacade, PCGExData::FFacade* InEdgeDataFacade) override;
		virtual bool Test(const PCGExCluster::FNode& Node) const override;

		bool TestDot(const PCGExCluster::FNode& Node) const;
		bool TestHash(const PCGExCluster::FNode& Node) const;

		virtual ~FEdgeDirectionFilter() override
		{
			TypedFilterFactory = nullptr;
		}
	};
}


/** Outputs a single GraphParam to be consumed by other nodes */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExEdgeDirectionFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		NodeEdgeDirectionFilterFactory, "Cluster Filter : Edge Direction", "Dot product comparison of connected edges against a direction attribute stored on the vtx.",
		PCGEX_FACTORY_NAME_PRIORITY)
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorClusterFilter; }
#endif

public:
	/** Test Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExEdgeDirectionFilterConfig Config;

public:
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
