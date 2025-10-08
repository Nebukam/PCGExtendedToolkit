﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Graph/PCGExClusterMT.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "Sampling/PCGExSampling.h"
#include "PCGExWriteVtxProperties.generated.h"

#define PCGEX_FOREACH_FIELD_VTXEXTRAS(MACRO) \
MACRO(VtxNormal, FVector, FVector::OneVector) \
MACRO(VtxEdgeCount, int32, 0)

class FPCGExVtxPropertyOperation;
class UPCGExVtxPropertyFactoryData;

namespace PCGExWriteVtxProperties
{
	class FBatch;
}

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(PCGExNodeLibraryDoc="metadata/vtx-properties"))
class UPCGExWriteVtxPropertiesSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(WriteVtxProperties, "Cluster : Vtx Properties", "Extract & write extra informations from the edges connected to the vtx.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->ColorNeighborSampler; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;

	/** Write normal from edges on vertices. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteVtxEdgeCount = false;

	/** Name of the 'normal' vertex attribute to write normal to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="EdgeCount", PCG_Overridable, EditCondition="bWriteVtxEdgeCount"))
	FName VtxEdgeCountAttributeName = FName("EdgeCount");

	/** Write normal from edges on vertices. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteVtxNormal = false;

	/** Name of the 'normal' vertex attribute to write normal to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Normal", PCG_Overridable, EditCondition="bWriteVtxNormal"))
	FName VtxNormalAttributeName = FName("Normal");

private:
	friend class FPCGExWriteVtxPropertiesElement;
};

struct FPCGExWriteVtxPropertiesContext final : FPCGExEdgesProcessorContext
{
	friend class FPCGExWriteVtxPropertiesElement;

	TArray<TObjectPtr<const UPCGExVtxPropertyFactoryData>> ExtraFactories;

	PCGEX_FOREACH_FIELD_VTXEXTRAS(PCGEX_OUTPUT_DECL_TOGGLE)

protected:
	PCGEX_ELEMENT_BATCH_EDGE_DECL
};

class FPCGExWriteVtxPropertiesElement final : public FPCGExEdgesProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(WriteVtxProperties)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

namespace PCGExWriteVtxProperties
{
	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExWriteVtxPropertiesContext, UPCGExWriteVtxPropertiesSettings>
	{
		friend class FBatch;

		TArray<TSharedPtr<FPCGExVtxPropertyOperation>> Operations;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessNodes(const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
		virtual void Cleanup() override;

		PCGEX_FOREACH_FIELD_VTXEXTRAS(PCGEX_OUTPUT_DECL)
	};

	class FBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
		PCGEX_FOREACH_FIELD_VTXEXTRAS(PCGEX_OUTPUT_DECL)

	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges);
		virtual ~FBatch() override;

		virtual void OnProcessingPreparationComplete() override;
		virtual bool PrepareSingle(const TSharedPtr<PCGExClusterMT::IProcessor>& InProcessor) override;
		virtual void Write() override;
	};
}
