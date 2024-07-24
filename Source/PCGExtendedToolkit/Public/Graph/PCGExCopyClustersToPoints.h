// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCluster.h"
#include "PCGExEdgesProcessor.h"

#include "PCGExCopyClustersToPoints.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExCopyClustersToPointsSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(CopyClustersToPoints, "Cluster : Copy to Points", "Create a copies of the input clusters onto the target points. \n NOTE: Does not sanitize input.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorCluster; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExEdgesProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;
	//~End UPCGExEdgesProcessorSettings interface

	/** Target inherit behavior */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExTransformDetails TransformDetails;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExCopyClustersToPointsContext final : public FPCGExEdgesProcessorContext
{
	friend class UPCGExCopyClustersToPointsSettings;
	friend class FPCGExCopyClustersToPointsElement;

	virtual ~FPCGExCopyClustersToPointsContext() override;

	FPCGExTransformDetails TransformDetails;

	PCGExData::FPointIO* Targets = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExCopyClustersToPointsElement final : public FPCGExEdgesProcessorElement
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

namespace PCGExCopyClusters
{
	class FProcessor final : public PCGExClusterMT::FClusterProcessor
	{
		FPCGExCopyClustersToPointsContext* LocalTypedContext = nullptr;
		
	public:
		TArray<PCGExData::FPointIO*>* VtxDupes = nullptr;
		TArray<FString>* VtxTag = nullptr;

		TArray<PCGExData::FPointIO*> EdgesDupes;

		FProcessor(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges):
			FClusterProcessor(InVtx, InEdges)
		{
			bBuildCluster = false;
		}

		virtual ~FProcessor() override;
		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void CompleteWork() override;
	};

	class FBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
		friend class FProcessor;

		FPCGExCopyClustersToPointsContext* LocalTypedContext = nullptr;
		
	public:
		TArray<PCGExData::FPointIO*> VtxDupes;
		TArray<FString> VtxTag;

		FBatch(FPCGContext* InContext, PCGExData::FPointIO* InVtx, const TArrayView<PCGExData::FPointIO*> InEdges):
			TBatch(InContext, InVtx, InEdges)
		{
		}

		virtual ~FBatch() override;
		virtual void Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual bool PrepareSingle(FProcessor* ClusterProcessor) override;
	};
}
