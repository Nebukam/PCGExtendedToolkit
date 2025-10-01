﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Details/PCGExSettingsMacros.h"
#include "Graph/Filters/PCGExAdjacency.h"

#include "Graph/PCGExCluster.h"
#include "Graph/Filters/PCGExClusterFilter.h"
#include "Misc/Filters/PCGExFilterFactoryProvider.h"

#include "PCGExNodeEdgeDirectionFilter.generated.h"


USTRUCT(BlueprintType)
struct FPCGExNodeEdgeDirectionFilterConfig
{
	GENERATED_BODY()

	FPCGExNodeEdgeDirectionFilterConfig() = default;

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
	EPCGExInputValueType CompareAgainst = EPCGExInputValueType::Constant;

	/** Operand B for testing -- Will be translated to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Direction (Attr)", EditCondition="CompareAgainst != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector Direction;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Invert", EditCondition="CompareAgainst != EPCGExInputValueType::Constant", EditConditionHides))
	bool bInvertDirection = false;

	/** Direction for computing the dot product against the edge's. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Direction", EditCondition="CompareAgainst == EPCGExInputValueType::Constant", EditConditionHides))
	FVector DirectionConstant = FVector::UpVector;

	/** Transform the reference direction with the local point' transform */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bTransformDirection = false;

	/** Dot comparison settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="ComparisonQuality == EPCGExDirectionCheckMode::Dot", EditConditionHides))
	FPCGExDotComparisonDetails DotComparisonDetails;

	/** Hash comparison settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="ComparisonQuality == EPCGExDirectionCheckMode::Hash", EditConditionHides))
	FPCGExVectorHashComparisonDetails HashComparisonDetails;

	PCGEX_SETTING_VALUE_DECL(Direction, FVector)
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExNodeEdgeDirectionFilterFactory : public UPCGExNodeFilterFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExNodeEdgeDirectionFilterConfig Config;

	virtual TSharedPtr<PCGExPointFilter::IFilter> CreateFilter() const override;
	virtual bool RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const override;
};

class FNodeEdgeDirectionFilter final : public PCGExClusterFilter::IVtxFilter
{
public:
	explicit FNodeEdgeDirectionFilter(const UPCGExNodeEdgeDirectionFilterFactory* InFactory)
		: IVtxFilter(InFactory), TypedFilterFactory(InFactory)
	{
		Adjacency = InFactory->Config.Adjacency;
		DotComparison = InFactory->Config.DotComparisonDetails;
		HashComparison = InFactory->Config.HashComparisonDetails;
	}

	const UPCGExNodeEdgeDirectionFilterFactory* TypedFilterFactory;

	bool bFromNode = true;
	bool bUseDot = true;
	double DirectionMultiplier = 1.0;

	FPCGExAdjacencySettings Adjacency;
	FPCGExDotComparisonDetails DotComparison;
	FPCGExVectorHashComparisonDetails HashComparison;

	TSharedPtr<PCGExDetails::TSettingValue<FVector>> OperandDirection;
	TConstPCGValueRange<FTransform> VtxTransforms;

	virtual bool Init(FPCGExContext* InContext, const TSharedRef<PCGExCluster::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade) override;
	virtual bool Test(const PCGExCluster::FNode& Node) const override;

	bool TestDot(const PCGExCluster::FNode& Node) const;
	bool TestHash(const PCGExCluster::FNode& Node) const;

	virtual ~FNodeEdgeDirectionFilter() override;
};


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params", meta=(PCGExNodeLibraryDoc="filters/filters-vtx-nodes/edge-direction"))
class UPCGExNodeEdgeDirectionFilterProviderSettings : public UPCGExVtxFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		NodeEdgeDirectionFilterFactory, "Vtx Filter : Edge Direction", "Dot product comparison of connected edges against a direction attribute stored on the vtx.",
		PCGEX_FACTORY_NAME_PRIORITY)
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->ColorFilterCluster); }
#endif

	virtual FName GetMainOutputPin() const override { return PCGExPointFilter::OutputFilterLabelNode; }

	/** Test Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExNodeEdgeDirectionFilterConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
