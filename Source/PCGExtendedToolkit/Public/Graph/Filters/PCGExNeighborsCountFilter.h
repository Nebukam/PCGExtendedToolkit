// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Data/PCGExGraphDefinition.h"
#include "Graph/PCGExCluster.h"
#include "Misc/Filters/PCGExFilterFactoryProvider.h"

#include "PCGExNeighborsCountFilter.generated.h"


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExNeighborsCountFilterDescriptor
{
	GENERATED_BODY()

	FPCGExNeighborsCountFilterDescriptor()
	{
	}

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExComparison Comparison = EPCGExComparison::NearlyEqual;

	/** Type of Count */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExOperandType CompareAgainst = EPCGExOperandType::Attribute;

	/** Operand A for testing -- Will be broadcasted to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="CompareAgainst==EPCGExOperandType::Attribute", EditConditionHides, ShowOnlyInnerProperties, DisplayName="Operand A (First)"))
	FPCGAttributePropertyInputSelector LocalCount;

	/** Constant Operand A for testing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CompareAgainst==EPCGExOperandType::Constant", EditConditionHides))
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
	EPCGExComparison Comparison = EPCGExComparison::NearlyEqual;
	EPCGExOperandType CompareAgainst = EPCGExOperandType::Attribute;
	FPCGAttributePropertyInputSelector LocalCount;
	double Count = 0;
	double Tolerance = 0.001;

	void ApplyDescriptor(const FPCGExNeighborsCountFilterDescriptor& Descriptor)
	{
		CompareAgainst = Descriptor.CompareAgainst;
		LocalCount = Descriptor.LocalCount;
		Count = Descriptor.Count;
		Comparison = Descriptor.Comparison;
		Tolerance = Descriptor.Tolerance;
	}

	virtual PCGExDataFilter::TFilter* CreateFilter() const override;
};

namespace PCGExNodeNeighborsCount
{
	class PCGEXTENDEDTOOLKIT_API TNeighborsCountFilter : public PCGExCluster::TClusterFilter
	{
	public:
		explicit TNeighborsCountFilter(const UPCGExNeighborsCountFilterFactory* InFactory)
			: TClusterFilter(InFactory), TypedFilterFactory(InFactory)
		{
		}

		const UPCGExNeighborsCountFilterFactory* TypedFilterFactory;

		PCGEx::FLocalSingleFieldGetter* LocalCount = nullptr;

		virtual PCGExDataFilter::EType GetFilterType() const override;

		virtual void Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO) override;
		virtual void CaptureEdges(const FPCGContext* InContext, const PCGExData::FPointIO* EdgeIO) override;

		FORCEINLINE virtual bool Test(const int32 PointIndex) const override;

		virtual ~TNeighborsCountFilter() override
		{
			PCGEX_DELETE(LocalCount)
		}
	};
}


/** Outputs a single GraphParam to be consumed by other nodes */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExNeighborsCountFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		NodeNeighborsCountFilterFactory, "Cluster Filter : Neighbors Count", "Check against the node' neighbor count.",
		PCGEX_FACTORY_NAME_PRIORITY)
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExEditorSettings>()->NodeColorClusterFilter; }
#endif
	//~End UPCGSettings

public:
	/** Test Descriptor.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExNeighborsCountFilterDescriptor Descriptor;

public:
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
