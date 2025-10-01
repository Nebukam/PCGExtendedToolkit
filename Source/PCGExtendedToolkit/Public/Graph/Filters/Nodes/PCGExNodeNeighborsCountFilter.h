﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "Details/PCGExSettingsMacros.h"

#include "Graph/PCGExCluster.h"
#include "Graph/Filters/PCGExClusterFilter.h"
#include "Misc/Filters/PCGExFilterFactoryProvider.h"

#include "PCGExNodeNeighborsCountFilter.generated.h"


USTRUCT(BlueprintType)
struct FPCGExNodeNeighborsCountFilterConfig
{
	GENERATED_BODY()

	FPCGExNodeNeighborsCountFilterConfig() = default;

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExComparison Comparison = EPCGExComparison::NearlyEqual;

	/** Type of Count */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType CompareAgainst = EPCGExInputValueType::Constant;

	/** Operand A for testing -- Will be translated to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Operand A (Attr)", EditCondition="CompareAgainst != EPCGExInputValueType::Constant", EditConditionHides, ShowOnlyInnerProperties))
	FPCGAttributePropertyInputSelector LocalCount;

	/** Constant Operand A for testing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Operand A", EditCondition="CompareAgainst == EPCGExInputValueType::Constant", EditConditionHides))
	int32 Count = 0;

	/** Rounding mode for near measures */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Comparison == EPCGExComparison::NearlyEqual || Comparison == EPCGExComparison::NearlyNotEqual", EditConditionHides))
	double Tolerance = DBL_COMPARE_TOLERANCE;

	PCGEX_SETTING_VALUE_DECL(LocalCount, double)
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExNodeNeighborsCountFilterFactory : public UPCGExNodeFilterFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExNodeNeighborsCountFilterConfig Config;

	virtual void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const override;
	virtual bool RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const override;

	virtual TSharedPtr<PCGExPointFilter::IFilter> CreateFilter() const override;
};

namespace PCGExNodeNeighborsCount
{
	class FFilter final : public PCGExClusterFilter::IVtxFilter
	{
	public:
		explicit FFilter(const UPCGExNodeNeighborsCountFilterFactory* InFactory)
			: IVtxFilter(InFactory), TypedFilterFactory(InFactory)
		{
		}

		const UPCGExNodeNeighborsCountFilterFactory* TypedFilterFactory;

		TSharedPtr<PCGExDetails::TSettingValue<double>> LocalCount;

		virtual bool Init(FPCGExContext* InContext, const TSharedRef<PCGExCluster::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade) override;
		virtual bool Test(const PCGExCluster::FNode& Node) const override;

		virtual ~FFilter() override;
	};
}


/** Outputs a single GraphParam to be consumed by other nodes */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params", meta=(PCGExNodeLibraryDoc="filters/filters-vtx-nodes/neighbors-count"))
class UPCGExNodeNeighborsCountFilterProviderSettings : public UPCGExVtxFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		NodeNeighborsCountFilterFactory, "Vtx Filter : Neighbors Count", "Check against the node' neighbor count.",
		PCGEX_FACTORY_NAME_PRIORITY)
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->ColorFilterCluster); }
#endif
	//~End UPCGSettings

	virtual FName GetMainOutputPin() const override { return PCGExPointFilter::OutputFilterLabelNode; }

	/** Test Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExNodeNeighborsCountFilterConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
