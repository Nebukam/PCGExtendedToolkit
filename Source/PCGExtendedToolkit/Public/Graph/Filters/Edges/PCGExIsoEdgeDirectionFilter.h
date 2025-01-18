// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/Filters/PCGExAdjacency.h"
#include "PCGExDetails.h"


#include "Graph/PCGExCluster.h"
#include "Graph/Filters/PCGExClusterFilter.h"
#include "Misc/Filters/PCGExFilterFactoryProvider.h"

#include "PCGExIsoEdgeDirectionFilter.generated.h"


USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExIsoEdgeDirectionFilterConfig
{
	GENERATED_BODY()

	FPCGExIsoEdgeDirectionFilterConfig()
	{
	}

	/** Defines the direction in which points will be ordered to form the final paths. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExEdgeDirectionSettings DirectionSettings;

	/** Type of check; Note that Fast comparison ignores adjacency consolidation. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExDirectionCheckMode ComparisonQuality = EPCGExDirectionCheckMode::Dot;

	/** Where to read the compared direction from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExInputValueType CompareAgainst = EPCGExInputValueType::Constant;

	/** Operand B for testing -- Will be translated to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Direction", EditCondition="CompareAgainst!=EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector Direction;

	/** Direction for computing the dot product against the edge's. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Direction", EditCondition="CompareAgainst==EPCGExInputValueType::Constant", EditConditionHides))
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
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExIsoEdgeDirectionFilterFactory : public UPCGExEdgeFilterFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExIsoEdgeDirectionFilterConfig Config;

	UPROPERTY()
	TArray<FPCGExSortRuleConfig> EdgeSortingRules;

	virtual void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const override;

	virtual TSharedPtr<PCGExPointFilter::FFilter> CreateFilter() const override;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FIsoEdgeDirectionFilter final : public PCGExClusterFilter::TEdgeFilter
{
public:
	explicit FIsoEdgeDirectionFilter(const UPCGExIsoEdgeDirectionFilterFactory* InFactory)
		: TEdgeFilter(InFactory), TypedFilterFactory(InFactory)
	{
		DotComparison = InFactory->Config.DotComparisonDetails;
		HashComparison = InFactory->Config.HashComparisonDetails;
	}

	const UPCGExIsoEdgeDirectionFilterFactory* TypedFilterFactory;

	bool bUseDot = true;

	TArray<double> CachedThreshold;
	FPCGExEdgeDirectionSettings DirectionSettings;
	FPCGExDotComparisonDetails DotComparison;
	FPCGExVectorHashComparisonDetails HashComparison;

	TSharedPtr<PCGExData::TBuffer<FVector>> OperandDirection;

	virtual bool Init(FPCGExContext* InContext, const TSharedRef<PCGExCluster::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade) override;
	virtual bool Test(const PCGExGraph::FEdge& Edge) const override;

	bool TestDot(const int32 PtIndex, const FVector& EdgeDir) const;
	bool TestHash(const int32 PtIndex, const FVector& EdgeDir) const;

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

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FName GetMainOutputPin() const override { return PCGExPointFilter::OutputFilterLabelEdge; }

	/** Test Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExIsoEdgeDirectionFilterConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

protected:
	virtual bool IsCacheable() const override { return true; }
};
