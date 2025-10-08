﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExMT.h"


#include "Graph/PCGExEdgesProcessor.h"
#include "PCGExUnpackClusters.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(PCGExNodeLibraryDoc="clusters/packing/unpack-cluster"))
class UPCGExUnpackClustersSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(UnpackClusters, "Cluster : Unpack", "Restores vtx/edge clusters from packed dataset.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->ColorClusterGenerator; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual FName GetMainInputPin() const override { return PCGExGraph::SourcePackedClustersLabel; }
	virtual FName GetMainOutputPin() const override { return PCGExGraph::OutputVerticesLabel; }
	//~End UPCGExPointsProcessorSettings

	/** Flatten unpacked metadata  Depending on your setup this is a tradeoff between memory and speed.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bFlatten = false;

private:
	friend class FPCGExUnpackClustersElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExUnpackClustersContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExUnpackClustersElement;

	TSharedPtr<PCGExData::FPointIOCollection> OutPoints;
	TSharedPtr<PCGExData::FPointIOCollection> OutEdges;
};

class FPCGExUnpackClustersElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(UnpackClusters)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

class FPCGExUnpackClusterTask final : public PCGExMT::FTask
{
public:
	PCGEX_ASYNC_TASK_NAME(FPCGExUnpackClusterTask)

	explicit FPCGExUnpackClusterTask(const TSharedPtr<PCGExData::FPointIO>& InPointIO)
		: FTask(),
		  PointIO(InPointIO)
	{
	}

	TSharedPtr<PCGExData::FPointIO> PointIO;
	virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
};
