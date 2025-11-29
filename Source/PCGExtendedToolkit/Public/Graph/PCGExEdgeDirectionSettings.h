// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdge.h"
#include "PCGExSorting.h"

#include "PCGExEdgeDirectionSettings.generated.h"

namespace PCGExCluster
{
	class FCluster;
}

namespace PCGExData
{
	class FFacadePreloader;

	template <typename T>
	class TBuffer;
}

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExEdgeDirectionSettings
{
	GENERATED_BODY()

	/** Method to pick the edge direction amongst various possibilities.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExEdgeDirectionMethod DirectionMethod = EPCGExEdgeDirectionMethod::EndpointsOrder;

	/** Attribute picker for the selected Direction Method.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="DirectionMethod == EPCGExEdgeDirectionMethod::EdgeDotAttribute", EditConditionHides))
	FPCGAttributePropertyInputSelector DirSourceAttribute;

	/** Further refine the direction method. Not all methods make use of this property.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExEdgeDirectionChoice DirectionChoice = EPCGExEdgeDirectionChoice::SmallestToGreatest;

	bool bAscendingDesired = false;
	TSharedPtr<PCGExData::TBuffer<FVector>> EdgeDirReader;

	TSharedPtr<PCGExSorting::FPointSorter> Sorter;

	void RegisterBuffersDependencies(
		FPCGExContext* InContext,
		PCGExData::FFacadePreloader& FacadePreloader,
		const TArray<FPCGExSortRuleConfig>* InSortingRules = nullptr) const;

	bool Init(
		FPCGExContext* InContext,
		const TSharedRef<PCGExData::FFacade>& InVtxDataFacade,
		const TArray<FPCGExSortRuleConfig>* InSortingRules = nullptr,
		const bool bQuiet = false);

	bool InitFromParent(
		FPCGExContext* InContext,
		const FPCGExEdgeDirectionSettings& ParentSettings,
		const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade,
		const bool bQuiet = false);

	bool RequiresSortingRules() const { return DirectionMethod == EPCGExEdgeDirectionMethod::EndpointsSort; }
	bool RequiresEndpointsMetadata() const { return DirectionMethod == EPCGExEdgeDirectionMethod::EndpointsSort; }
	bool RequiresEdgeMetadata() const { return DirectionMethod == EPCGExEdgeDirectionMethod::EdgeDotAttribute; }

	bool SortEndpoints(const PCGExCluster::FCluster* InCluster, PCGExGraph::FEdge& InEdge) const;
	bool SortExtrapolation(const PCGExCluster::FCluster* InCluster, const int32 InEdgeIndex, const int32 StartNodeIndex, const int32 EndNodeIndex) const;
};
