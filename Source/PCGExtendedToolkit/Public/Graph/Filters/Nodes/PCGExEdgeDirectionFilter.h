// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/Filters/PCGExAdjacency.h"
#include "PCGExSettings.h"


#include "Graph/PCGExCluster.h"
#include "Misc/Filters/PCGExFilterFactoryProvider.h"

#include "PCGExEdgeDirectionFilter.generated.h"


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExEdgeDirectionFilterDescriptor
{
	GENERATED_BODY()

	FPCGExEdgeDirectionFilterDescriptor()
	{
	}

	/** Adjacency Settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExAdjacencySettings Adjacency;
	
	/** Direction orientation */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExAdjacencyDirectionOrigin Origin = EPCGExAdjacencyDirectionOrigin::FromNode;

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExComparison Comparison = EPCGExComparison::NearlyEqual;

	/** Where to read the compared direction from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExFetchType CompareAgainst = EPCGExFetchType::Attribute;

	/** Operand B for testing -- Will be translated to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CompareAgainst==EPCGExFetchType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector Direction;

	/** Direction for computing the dot product against the edge's. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CompareAgainst==EPCGExFetchType::Constant", EditConditionHides))
	FVector DirectionConstant = FVector::UpVector;

	/** Transform the reference direction with the local point' transform */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bTransformDirection = false;

	/** Dot comparison settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExDotComparisonSettings DotComparisonSettings;
};

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExEdgeDirectionFilterFactory : public UPCGExClusterFilterFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExEdgeDirectionFilterDescriptor Descriptor;

	virtual PCGExDataFilter::TFilter* CreateFilter() const override;
};

namespace PCGExNodeAdjacency
{
	class PCGEXTENDEDTOOLKIT_API TEdgeDirectionFilter final : public PCGExCluster::TClusterNodeFilter
	{
	public:
		explicit TEdgeDirectionFilter(const UPCGExEdgeDirectionFilterFactory* InFactory)
			: TClusterNodeFilter(InFactory), TypedFilterFactory(InFactory)
		{
			Adjacency = InFactory->Descriptor.Adjacency;
			DotComparison = InFactory->Descriptor.DotComparisonSettings;
		}

		const UPCGExEdgeDirectionFilterFactory* TypedFilterFactory;

		bool bFromNode = true;
		
		TArray<double> CachedThreshold;
		FPCGExAdjacencySettings Adjacency;
		FPCGExDotComparisonSettings DotComparison;

		PCGExDataCaching::FCache<FVector>* OperandDirection = nullptr;

		virtual PCGExDataFilter::EType GetFilterType() const override;

		virtual void Capture(const FPCGContext* InContext, PCGExDataCaching::FPool* InPrimaryDataCache) override;
		//virtual void CaptureEdges(const FPCGContext* InContext, const PCGExData::FPointIO* EdgeIO) override;

		virtual void PrepareForTesting() override;
		FORCEINLINE virtual bool Test(const int32 PointIndex) const override;

		virtual ~TEdgeDirectionFilter() override
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
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		NodeEdgeDirectionFilterFactory, "Cluster Filter : Edge Direction", "Dot product comparison of connected edges against a direction attribute stored on the vtx.",
		PCGEX_FACTORY_NAME_PRIORITY)
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorClusterFilter; }
#endif

public:
	/** Test Descriptor.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExEdgeDirectionFilterDescriptor Descriptor;

public:
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
