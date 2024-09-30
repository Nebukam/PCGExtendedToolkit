// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCluster.h"
#include "PCGExEdgesProcessor.h"


#include "PCGExCopyClustersToPoints.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExCopyClustersToPointsSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(CopyClustersToPoints, "Cluster : Copy to Points", "Create a copies of the input clusters onto the target points.  NOTE: Does not sanitize input.");
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

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExCopyClustersToPointsContext final : public FPCGExEdgesProcessorContext
{
	friend class UPCGExCopyClustersToPointsSettings;
	friend class FPCGExCopyClustersToPointsElement;

	FPCGExTransformDetails TransformDetails;

	TSharedPtr<PCGExData::FPointIO> Targets;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExCopyClustersToPointsElement final : public FPCGExEdgesProcessorElement
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
	class FProcessor final : public PCGExClusterMT::TClusterProcessor<FPCGExCopyClustersToPointsContext, UPCGExCopyClustersToPointsSettings>
	{
	public:
		TArray<TSharedPtr<PCGExData::FPointIO>>* VtxDupes = nullptr;
		TArray<FString>* VtxTag = nullptr;

		TArray<TSharedPtr<PCGExData::FPointIO>> EdgesDupes;

		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade):
			TClusterProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
			bBuildCluster = false;
		}

		virtual ~FProcessor() override;
		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void CompleteWork() override;
	};

	class FBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
		friend class FProcessor;

	public:
		TArray<TSharedPtr<PCGExData::FPointIO>> VtxDupes;
		TArray<FString> VtxTag;

		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges):
			TBatch(InContext, InVtx, InEdges)
		{
		}

		virtual ~FBatch() override;
		virtual void Process() override;
		virtual bool PrepareSingle(const TSharedPtr<FProcessor>& ClusterProcessor) override;
	};
}
