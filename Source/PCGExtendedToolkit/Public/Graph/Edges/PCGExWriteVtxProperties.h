// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Graph/PCGExClusterMT.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "Sampling/PCGExSampling.h"
#include "PCGExWriteVtxProperties.generated.h"

#define PCGEX_FOREACH_FIELD_VTXEXTRAS(MACRO) \
MACRO(VtxNormal, FVector) \
MACRO(VtxEdgeCount, int32)
class UPCGExVtxPropertyOperation;
class UPCGExVtxPropertyFactoryBase;

namespace PCGExWriteVtxProperties
{
	class FProcessorBatch;
}

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Edges")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExWriteVtxPropertiesSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(WriteVtxProperties, "Cluster : Vtx Properties", "Extract & write extra informations from the edges connected to the vtx.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorSamplerNeighbor; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;

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

private:
	friend class FPCGExWriteVtxPropertiesElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExWriteVtxPropertiesContext final : public FPCGExEdgesProcessorContext
{
	friend class FPCGExWriteVtxPropertiesElement;

	virtual ~FPCGExWriteVtxPropertiesContext() override;

	TArray<UPCGExVtxPropertyFactoryBase*> ExtraFactories;

	PCGEX_FOREACH_FIELD_VTXEXTRAS(PCGEX_OUTPUT_DECL_TOGGLE)
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExWriteVtxPropertiesElement final : public FPCGExEdgesProcessorElement
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

namespace PCGExWriteVtxProperties
{
	class FProcessor final : public PCGExClusterMT::FClusterProcessor
	{
		friend class FProcessorBatch;

		TArray<UPCGExVtxPropertyOperation*>* ExtraOperations = nullptr;

	public:
		FProcessor(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges):
			FClusterProcessor(InVtx, InEdges)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node) override;
		virtual void CompleteWork() override;

		PCGEX_FOREACH_FIELD_VTXEXTRAS(PCGEX_OUTPUT_DECL)
	};

	class FProcessorBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
		TArray<UPCGExVtxPropertyOperation*> ExtraOperations;

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
