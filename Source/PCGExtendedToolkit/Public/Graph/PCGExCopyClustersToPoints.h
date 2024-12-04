// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCluster.h"
#include "PCGExEdgesProcessor.h"
#include "Data/PCGExDataForward.h"


#include "PCGExCopyClustersToPoints.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
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
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;
	//~End UPCGExEdgesProcessorSettings interface

	/** Target inherit behavior */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExTransformDetails TransformDetails;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding")
	FPCGExAttributeToTagDetails TargetsAttributesToPathTags;

	/** Which Seed attributes to forward on paths. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding")
	FPCGExForwardDetails TargetsForwarding;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExCopyClustersToPointsContext final : FPCGExEdgesProcessorContext
{
	friend class UPCGExCopyClustersToPointsSettings;
	friend class FPCGExCopyClustersToPointsElement;

	FPCGExTransformDetails TransformDetails;

	TSharedPtr<PCGExData::FFacade> TargetsDataFacade;

	FPCGExAttributeToTagDetails TargetsAttributesToPathTags;
	TSharedPtr<PCGExData::FDataForwardHandler> TargetsForwardHandler;
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
	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExCopyClustersToPointsContext, UPCGExCopyClustersToPointsSettings>
	{
	public:
		TArray<TSharedPtr<PCGExData::FPointIO>>* VtxDupes = nullptr;
		TArray<FString>* VtxTag = nullptr;

		TArray<TSharedPtr<PCGExData::FPointIO>> EdgesDupes;

		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade):
			TProcessor(InVtxDataFacade, InEdgeDataFacade)
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
