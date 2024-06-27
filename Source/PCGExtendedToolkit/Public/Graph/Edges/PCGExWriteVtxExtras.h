// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Graph/PCGExClusterMT.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "Sampling/PCGExSampling.h"
#include "PCGExWriteVtxExtras.generated.h"

#define PCGEX_FOREACH_FIELD_VTXEXTRAS(MACRO) \
MACRO(VtxNormal, FVector) \
MACRO(VtxEdgeCount, int32)
class UPCGExVtxExtraOperation;
class UPCGExVtxExtraFactoryBase;

namespace PCGExWriteVtxExtras
{
	class FProcessorBatch;
}

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Edges")
class PCGEXTENDEDTOOLKIT_API UPCGExWriteVtxExtrasSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(WriteVtxExtras, "Vtx : Write Extras", "Extract & write extra informations from the edges connected to the vtx.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorSamplerNeighbor; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** Write normal from edges on vertices. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteVtxEdgeCount = false;

	/** Name of the 'normal' vertex attribute to write normal to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteVtxEdgeCount"))
	FName VtxEdgeCountAttributeName = FName("EdgeCount");

	/** Write normal from edges on vertices. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteVtxNormal = false;

	/** Name of the 'normal' vertex attribute to write normal to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteVtxNormal"))
	FName VtxNormalAttributeName = FName("Normal");

	/** Projection settings used for normal calculations. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta = (PCG_Overridable, DisplayName="Normal Projection", EditCondition="bWriteVtxNormal"))
	FPCGExGeo2DProjectionSettings ProjectionSettings;

private:
	friend class FPCGExWriteVtxExtrasElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExWriteVtxExtrasContext final : public FPCGExEdgesProcessorContext
{
	friend class FPCGExWriteVtxExtrasElement;

	virtual ~FPCGExWriteVtxExtrasContext() override;

	TArray<UPCGExVtxExtraFactoryBase*> ExtraFactories;

	PCGEX_FOREACH_FIELD_VTXEXTRAS(PCGEX_OUTPUT_DECL_TOGGLE)
};

class PCGEXTENDEDTOOLKIT_API FPCGExWriteVtxExtrasElement final : public FPCGExEdgesProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

namespace PCGExWriteVtxExtras
{
	class FProcessor final : public PCGExClusterMT::FClusterProcessor
	{
		friend class FProcessorBatch;

		PCGExCluster::FClusterProjection* ProjectedCluster = nullptr;
		TArray<UPCGExVtxExtraOperation*>* ExtraOperations = nullptr;

	public:
		FProcessor(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges):
			FClusterProcessor(InVtx, InEdges)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void ProcessSingleRangeIteration(const int32 Iteration) override;
		virtual void ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node) override;
		virtual void CompleteWork() override;

		PCGEX_FOREACH_FIELD_VTXEXTRAS(PCGEX_OUTPUT_DECL)

		FPCGExGeo2DProjectionSettings* ProjectionSettings = nullptr;
	};

	class FProcessorBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
		FPCGExGeo2DProjectionSettings ProjectionSettings;

		TArray<UPCGExVtxExtraOperation*> ExtraOperations;

		PCGEX_FOREACH_FIELD_VTXEXTRAS(PCGEX_OUTPUT_DECL)

	public:
		FProcessorBatch(FPCGContext* InContext, PCGExData::FPointIO* InVtx, TArrayView<PCGExData::FPointIO*> InEdges);
		virtual ~FProcessorBatch() override;

		virtual bool PrepareProcessing() override;
		virtual bool PrepareSingle(FProcessor* ClusterProcessor) override;
		//virtual void CompleteWork() override;
		virtual void Write() override;
	};
}
