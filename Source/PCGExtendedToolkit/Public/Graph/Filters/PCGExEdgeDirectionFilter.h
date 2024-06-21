// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExAdjacency.h"
#include "PCGExSettings.h"

#include "Data/PCGExGraphDefinition.h"
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

	/** How many adjacent items should be tested. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExAdjacencyTestMode Mode = EPCGExAdjacencyTestMode::All;

	/** How to consolidate value for testing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="Mode==EPCGExAdjacencyTestMode::All", EditConditionHides))
	EPCGExAdjacencyGatherMode Consolidation = EPCGExAdjacencyGatherMode::Average;

	/** How should adjacency be observed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="Mode==EPCGExAdjacencyTestMode::Some", EditConditionHides))
	EPCGExAdjacencySubsetMode SubsetMode = EPCGExAdjacencySubsetMode::AtLeast;

	/** Define the nodes subset' size that must meet requirements. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="Mode==EPCGExAdjacencyTestMode::Some", EditConditionHides))
	EPCGExMeanMeasure SubsetMeasure = EPCGExMeanMeasure::Absolute;

	/** Define the nodes subset' size that must meet requirements. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="Mode==EPCGExAdjacencyTestMode::Some", EditConditionHides))
	EPCGExFetchType SubsetSource = EPCGExFetchType::Constant;

	/** Local measure attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="Mode==EPCGExAdjacencyTestMode::Some && SubsetSource==EPCGExFetchType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector LocalMeasure;

	/** Constant Local measure value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="Mode==EPCGExAdjacencyTestMode::Some && SubsetSource==EPCGExFetchType::Constant", EditConditionHides))
	double ConstantMeasure = 0;

	/** Type of OperandA */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExFetchType CompareAgainst = EPCGExFetchType::Attribute;

	/** Operand A for testing -- Will be translated to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="CompareAgainst==EPCGExFetchType::Attribute", EditConditionHides, ShowOnlyInnerProperties, DisplayName="Operand A (First)"))
	FPCGAttributePropertyInputSelector OperandA;

	/** Constant Operand A for testing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CompareAgainst==EPCGExFetchType::Constant", EditConditionHides))
	double OperandAConstant = 0;

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExComparison Comparison = EPCGExComparison::NearlyEqual;

	/** Source of the Operand B value -- either the neighboring point, or the edge connecting to that point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExGraphValueSource OperandBSource = EPCGExGraphValueSource::Point;

	/** Operand B for testing -- Will be translated to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(ShowOnlyInnerProperties, DisplayName="Operand B (Neighbor)"))
	FPCGAttributePropertyInputSelector OperandB;

	/** Rounding mode for near measures */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Comparison==EPCGExComparison::NearlyEqual || Comparison==EPCGExComparison::NearlyNotEqual", EditConditionHides))
	double Tolerance = 0.001;
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
	class PCGEXTENDEDTOOLKIT_API TEdgeDirectionFilter final : public PCGExCluster::TClusterFilter
	{
	public:
		explicit TEdgeDirectionFilter(const UPCGExEdgeDirectionFilterFactory* InFactory)
			: TClusterFilter(InFactory), TypedFilterFactory(InFactory)
		{
		}

		const UPCGExEdgeDirectionFilterFactory* TypedFilterFactory;

		TArray<double> CachedMeasure;

		bool bUseAbsoluteMeasure = false;
		bool bUseLocalMeasure = false;
		PCGEx::FLocalSingleFieldGetter* LocalMeasure = nullptr;
		PCGEx::FLocalSingleFieldGetter* OperandA = nullptr;
		PCGEx::FLocalSingleFieldGetter* OperandB = nullptr;

		virtual PCGExDataFilter::EType GetFilterType() const override;

		virtual void Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO) override;
		virtual void CaptureEdges(const FPCGContext* InContext, const PCGExData::FPointIO* EdgeIO) override;

		virtual bool PrepareForTesting(const PCGExData::FPointIO* PointIO) override;
		FORCEINLINE virtual bool Test(const int32 PointIndex) const override;

		virtual ~TEdgeDirectionFilter() override
		{
			PCGEX_DELETE(LocalMeasure)
			PCGEX_DELETE(OperandA)
			PCGEX_DELETE(OperandB)
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
