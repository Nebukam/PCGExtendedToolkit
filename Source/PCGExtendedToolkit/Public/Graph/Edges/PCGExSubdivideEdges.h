// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Graph/PCGExEdgesProcessor.h"
#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendOperation.h"
#include "PCGExSubdivideEdges.generated.h"

UCLASS(Abstract, MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(PCGExNodeLibraryDoc="TBD"))
class UPCGExSubdivideEdgesSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

	//~Begin UObject interface
public:
	//~End UObject interface

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SubdivideEdges, "Cluster : Subdivide Edges", "Subdivide edges.");
#endif
	virtual bool SupportsEdgeSorting() const override { return DirectionSettings.RequiresSortingRules(); }
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** Defines the direction of an edge, and which endpoints should be considered the start & end. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExEdgeDirectionSettings DirectionSettings;

	/** Reference for computing the blending interpolation point point */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExSubdivideMode SubdivideMethod = EPCGExSubdivideMode::Distance;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExInputValueType AmountInput = EPCGExInputValueType::Constant;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Subdivision (Distance)", EditCondition="SubdivideMethod == EPCGExSubdivideMode::Distance && AmountInput == EPCGExInputValueType::Constant", EditConditionHides, ClampMin=0.1))
	double Distance = 10;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Subdivision (Count)", EditCondition="SubdivideMethod == EPCGExSubdivideMode::Count && AmountInput == EPCGExInputValueType::Constant", EditConditionHides, ClampMin=1))
	int32 Count = 10;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="AmountInput != EPCGExInputValueType::Constant", EditConditionHides, ClampMin=1))
	EPCGExClusterElement AmountSource = EPCGExClusterElement::Edge;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Subdivision (Attr)", EditCondition="AmountInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector SubdivisionAmount;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta=(PCG_Overridable, ShowOnlyInnerProperties, NoResetToDefault))
	TObjectPtr<UPCGExSubPointsBlendInstancedFactory> Blending;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bFlagSubVtx = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(PCG_Overridable, EditCondition="bFlagSubVtx"))
	FName SubVtxFlagName = "IsSubVtx";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bFlagSubEdge = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(PCG_Overridable, EditCondition="bFlagSubEdge"))
	FName SubEdgeFlagName = "IsSubEdge";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteVtxAlpha = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(PCG_Overridable, EditCondition="bWriteVtxAlpha"))
	FName VtxAlphaAttributeName = "Alpha";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(PCG_Overridable, EditCondition="bWriteVtxAlpha", EditConditionHides, HideEditConditionToggle))
	double DefaultVtxAlpha = 1;

private:
	friend class FPCGExSubdivideEdgesElement;
};

struct FPCGExSubdivideEdgesContext final : FPCGExEdgesProcessorContext
{
	friend class FPCGExSubdivideEdgesElement;

	UPCGExSubPointsBlendInstancedFactory* Blending = nullptr;
};

class FPCGExSubdivideEdgesElement final : public FPCGExEdgesProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(SubdivideEdges)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

namespace PCGExSubdivideEdges
{
	struct FSubdivision
	{
		FSubdivision() = default;

		// Cluster Edge is being mutated during sorting so no need to store extra indices.
		
		int32 NumSubdivisions = 0;
		int32 StartNodeIndex = -1; // Also the point index stored in the node
		
		FVector Dir = FVector::ZeroVector;
	};

	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExSubdivideEdgesContext, UPCGExSubdivideEdgesSettings>
	{
		TArray<FSubdivision> Subdivisions;

		TSet<FName> ProtectedAttributes;
		TSharedPtr<FPCGExSubPointsBlendOperation> SubBlending;

		TSharedPtr<PCGExData::TBuffer<bool>> FlagWriter;
		TSharedPtr<PCGExData::TBuffer<double>> AlphaWriter;
		TSharedPtr<PCGExData::TBuffer<double>> AmountGetter;

		double ConstantAmount = 0;
		int32 NewNodesNum = 0;

		bool bUseCount = false;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual TSharedPtr<PCGExCluster::FCluster> HandleCachedCluster(const TSharedRef<PCGExCluster::FCluster>& InClusterRef) override;
		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessEdges(const PCGExMT::FScope& Scope) override;
		virtual void OnEdgesProcessingComplete() override;
		
		virtual void CompleteWork() override;
		virtual void Write() override;
	};

	class FBatch final : public PCGExClusterMT::TBatchWithGraphBuilder<FProcessor>
	{
		friend class FProcessor;

		FPCGExEdgeDirectionSettings DirectionSettings;

	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges)
			: TBatchWithGraphBuilder(InContext, InVtx, InEdges)
		{
		}

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override;
		virtual void OnProcessingPreparationComplete() override;
	};
}
