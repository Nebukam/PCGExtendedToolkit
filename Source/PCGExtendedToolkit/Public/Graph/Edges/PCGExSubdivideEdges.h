// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Graph/PCGExEdgesProcessor.h"
#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendOperation.h"
#include "PCGExSubdivideEdges.generated.h"

UCLASS(Abstract, MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExSubdivideEdgesSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

	//~Begin UObject interface
public:
	//~End UObject interface

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SubdivideEdges, "Cluster : Subdivide Edges", "Subdivide edges.");
#endif

	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;

protected:
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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="SubdivideMethod==EPCGExSubdivideMode::Distance && AmountInput==EPCGExInputValueType::Constant", EditConditionHides, ClampMin=0.1))
	double Distance = 10;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="SubdivideMethod==EPCGExSubdivideMode::Count && AmountInput==EPCGExInputValueType::Constant", EditConditionHides, ClampMin=1))
	int32 Count = 10;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="AmountInput==EPCGExInputValueType::Attribute", EditConditionHides, ClampMin=1))
	EPCGExClusterComponentSource AmountSource = EPCGExClusterComponentSource::Edge;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="AmountInput==EPCGExInputValueType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector SubdivisionAmount;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta=(PCG_Overridable, ShowOnlyInnerProperties, NoResetToDefault))
	TObjectPtr<UPCGExSubPointsBlendOperation> Blending;

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

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSubdivideEdgesContext final : FPCGExEdgesProcessorContext
{
	friend class FPCGExSubdivideEdgesElement;

	UPCGExSubPointsBlendOperation* Blending = nullptr;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSubdivideEdgesElement final : public FPCGExEdgesProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

namespace PCGExSubdivideEdges
{
	struct FSubdivision
	{
		int32 NumSubdivisions = 0;
		int32 OutStart = -1;
		int32 OutEnd = -1;
		double Dist = 0;
		double StepSize = 0;
		double StartOffset = 0;
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		FVector Dir = FVector::ZeroVector;
	};

	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExSubdivideEdgesContext, UPCGExSubdivideEdgesSettings>
	{
		TArray<FSubdivision> Subdivisions;

		TSet<FName> ProtectedAttributes;
		UPCGExSubPointsBlendOperation* Blending = nullptr;

		TSharedPtr<PCGExData::TBuffer<bool>> FlagWriter;
		TSharedPtr<PCGExData::TBuffer<double>> AlphaWriter;

		TSharedPtr<PCGExData::TBuffer<double>> AmountGetter;

		double ConstantAmount = 0;

		bool bUseCount = false;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual TSharedPtr<PCGExCluster::FCluster> HandleCachedCluster(const TSharedRef<PCGExCluster::FCluster>& InClusterRef) override;
		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FIndexedEdge& Edge, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};

	class FProcessorBatch final : public PCGExClusterMT::TBatchWithGraphBuilder<FProcessor>
	{
		friend class FProcessor;

		FPCGExEdgeDirectionSettings DirectionSettings;

	public:
		FProcessorBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges)
			: TBatchWithGraphBuilder<FProcessor>(InContext, InVtx, InEdges)
		{
			bAllowVtxDataFacadeScopedGet = true;
		}

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override;
		virtual void OnProcessingPreparationComplete() override;
	};
}
