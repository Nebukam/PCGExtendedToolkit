// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExFactoryProvider.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Graph/PCGExCluster.h"
#include "Graph/PCGExGraph.h"

#include "PCGExNeighborSampleFactoryProvider.h"
#include "Data/Blending/PCGExMetadataBlender.h"


#include "PCGExNeighborSampleFilters.generated.h"

///

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSamplerFilterConfig
{
	GENERATED_BODY()

	FPCGExSamplerFilterConfig()
	{
	}

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteInsideNum = false;

	/** Name of the attribute to write the number of tests that passed (inside filters) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName = "Inside Num", EditCondition="bWriteInsideNum"))
	FName InsideNumAttributeName = FName(FName("InsideNum"));

	/** If enabled, outputs the value divided by the total number of samples */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName = " └─ Normalize", EditCondition="bWriteInsideNum", EditConditionHides, HideEditConditionToggle))
	bool bNormalizeInsideNum = false;
	
	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle, InlineEditConditionToggle))
	bool bWriteOutsideNum = false;

	/** Name of the attribute to write the number of tests that failed (outside filters) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName = "Outside Num", EditCondition="bWriteOutsideNum"))
	FName OutsideNumAttributeName = FName(FName("OutsideNum"));

	/** If enabled, outputs the value divided by the total number of samples */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName = " └─ Normalize", EditCondition="bWriteOutsideNum", EditConditionHides, HideEditConditionToggle))
	bool bNormalizeOutsideNum = false;
	
	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteTotalNum = false;

	/** Name of the attribute to write the total number of points tested */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName = "Total Num", EditCondition="bWriteTotalNum"))
	FName TotalNumAttributeName = FName(FName("TotalNum"));



	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteInsideWeight = false;

	/** Name of the attribute to write the number of tests weight that passed (inside filters) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName = "Inside Weight", EditCondition="bWriteInsideWeight"))
	FName InsideWeightAttributeName = FName(FName("InsideWeight"));

	/** If enabled, outputs the value divided by the total weight of samples */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName = " └─ Normalize", EditCondition="bWriteInsideWeight", EditConditionHides, HideEditConditionToggle))
	bool bNormalizeInsideWeight = false;

	
	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteOutsideWeight = false;

	/** Name of the attribute to write the number of tested weight that passed (inside filters) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName = "Outside Weight", EditCondition="bWriteOutsideWeight"))
	FName OutsideWeightAttributeName = FName(FName("OutsideWeight"));

	/** If enabled, outputs the value divided by the total weight of samples */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName = " └─ Normalize", EditCondition="bWriteOutsideWeight", EditConditionHides, HideEditConditionToggle))
	bool bNormalizeOutsideWeight = false;

	
	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteTotalWeight = false;

	/** Name of the attribute to write the total weight tested */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName = "Total Weight", EditCondition="bWriteTotalWeight"))
	FName TotalWeightAttributeName = FName(FName("TotalWeight"));
};

/**
 * 
 */
UCLASS(MinimalAPI)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExNeighborSampleFilters : public UPCGExNeighborSampleOperation
{
	GENERATED_BODY()

public:
	FPCGExSamplerFilterConfig Config;
	TSharedPtr<PCGExClusterFilter::FManager> FilterManager;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual void PrepareForCluster(FPCGExContext* InContext, TSharedRef<PCGExCluster::FCluster> InCluster, TSharedRef<PCGExData::FFacade> InVtxDataFacade, TSharedRef<PCGExData::FFacade> InEdgeDataFacade) override;

	FORCEINLINE virtual void PrepareNode(const PCGExCluster::FNode& TargetNode) const override
	{
	}

	FORCEINLINE virtual void SampleNeighborNode(const PCGExCluster::FNode& TargetNode, const PCGExGraph::FLink Lk, const double Weight) override
	{
		if (FilterManager->Test(*Cluster->GetNode(Lk)))
		{
			Inside[TargetNode.Index] += 1;
			InsideWeight[TargetNode.Index] += Weight;
		}
		else
		{
			Outside[TargetNode.Index] += 1;
			OutsideWeight[TargetNode.Index] += Weight;
		}
	}

	FORCEINLINE virtual void SampleNeighborEdge(const PCGExCluster::FNode& TargetNode, const PCGExGraph::FLink Lk, const double Weight) override
	{
		if (FilterManager->Test(*Cluster->GetEdge(Lk)))
		{
			Inside[TargetNode.Index] += 1;
			InsideWeight[TargetNode.Index] += Weight;
		}
		else
		{
			Outside[TargetNode.Index] += 1;
			OutsideWeight[TargetNode.Index] += Weight;
		}
	}

	FORCEINLINE virtual void FinalizeNode(const PCGExCluster::FNode& TargetNode, const int32 Count, const double TotalWeight) override
	{
		const int32 WriteIndex = TargetNode.PointIndex;
		const int32 ReadIndex = TargetNode.Index;
		
		if (NumInsideBuffer) { NumInsideBuffer->GetMutable(WriteIndex) = Inside[ReadIndex]; }
		else if (NormalizedNumInsideBuffer) { NormalizedNumInsideBuffer->GetMutable(WriteIndex) = static_cast<double>(Inside[ReadIndex]) / static_cast<double>(Count); }

		if (NumOutsideBuffer) { NumOutsideBuffer->GetMutable(WriteIndex) = Outside[ReadIndex]; }
		else if (NormalizedNumOutsideBuffer) { NormalizedNumOutsideBuffer->GetMutable(WriteIndex) = static_cast<double>(Outside[ReadIndex]) / static_cast<double>(Count); }

		if (TotalNumBuffer) { TotalNumBuffer->GetMutable(WriteIndex) = Count; }

		if (WeightInsideBuffer) { WeightInsideBuffer->GetMutable(WriteIndex) = InsideWeight[ReadIndex]; }
		else if (NormalizedWeightInsideBuffer) { NormalizedWeightInsideBuffer->GetMutable(WriteIndex) = InsideWeight[ReadIndex] / TotalWeight; }

		if (WeightOutsideBuffer) { WeightOutsideBuffer->GetMutable(WriteIndex) = Outside[ReadIndex]; }
		else if (NormalizedWeightOutsideBuffer) { NormalizedWeightOutsideBuffer->GetMutable(WriteIndex) = OutsideWeight[ReadIndex] / TotalWeight; }

		if (TotalWeightBuffer) { TotalWeightBuffer->GetMutable(WriteIndex) = TotalWeight; }
	}

	virtual void CompleteOperation() override;

	virtual void Cleanup() override;

protected:
	TArray<int32> Inside;
	TArray<double> InsideWeight;
	TArray<int32> Outside;
	TArray<double> OutsideWeight;

	TSharedPtr<PCGExData::TBuffer<int32>> NumInsideBuffer;
	TSharedPtr<PCGExData::TBuffer<double>> NormalizedNumInsideBuffer;
	TSharedPtr<PCGExData::TBuffer<double>> WeightInsideBuffer;
	TSharedPtr<PCGExData::TBuffer<double>> NormalizedWeightInsideBuffer;
	
	TSharedPtr<PCGExData::TBuffer<int32>> NumOutsideBuffer;
	TSharedPtr<PCGExData::TBuffer<double>> NormalizedNumOutsideBuffer;
	TSharedPtr<PCGExData::TBuffer<double>> WeightOutsideBuffer;
	TSharedPtr<PCGExData::TBuffer<double>> NormalizedWeightOutsideBuffer;
	
	TSharedPtr<PCGExData::TBuffer<int32>> TotalNumBuffer;
	TSharedPtr<PCGExData::TBuffer<int32>> TotalWeightBuffer;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExNeighborSamplerFactoryFilters : public UPCGExNeighborSamplerFactoryData
{
	GENERATED_BODY()

public:
	FPCGExSamplerFilterConfig Config;
	virtual UPCGExNeighborSampleOperation* CreateOperation(FPCGExContext* InContext) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|NeighborSample")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExNeighborSampleFiltersSettings : public UPCGExNeighborSampleProviderSettings
{
	GENERATED_BODY()

public:
	UPCGExNeighborSampleFiltersSettings(const FObjectInitializer& ObjectInitializer);

protected:
	virtual bool SupportsVtxFilters(bool& bIsRequired) const override;
	virtual bool SupportsEdgeFilters(bool& bIsRequired) const override;

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(NeighborSamplerFilters, "Sampler : Test Neighbors", "Writes the number of neighbors that pass the provided filters")

#endif
	//~End UPCGSettings

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

	/** Sampler Settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExSamplerFilterConfig Config;
};
