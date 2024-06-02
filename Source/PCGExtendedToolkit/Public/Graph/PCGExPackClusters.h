// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "PCGExPackClusters.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Edges")
class PCGEXTENDEDTOOLKIT_API UPCGExPackClustersSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExPackClustersSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PackClusters, "Graph : Pack Clusters", "Pack each cluster into an single point data object containing both vtx and edges.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExEditorSettings>()->NodeColorGraph; }
#endif

	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;

	/** Flatten packed metadata \n Depending on your setup this is a tradeoff between memory and speed.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bFlatten = false;

private:
	friend class FPCGExPackClustersElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPackClustersContext : public FPCGExEdgesProcessorContext
{
	friend class FPCGExPackClustersElement;
	friend class FPCGExCreateBridgeTask;

	virtual ~FPCGExPackClustersContext() override;

	PCGEx::FAttributesInfos* VtxAttributes;
	PCGExData::FPointIOCollection* PackedClusters = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPackClustersElement : public FPCGExEdgesProcessorElement
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

class PCGEXTENDEDTOOLKIT_API FPCGExPackClusterTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExPackClusterTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
	                      PCGExData::FPointIO* InInVtx,
	                      PCGExData::FPointIO* InInEdges) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		InVtx(InInVtx),
		InEdges(InInEdges)
	{
	}

	PCGExData::FPointIO* InVtx = nullptr;
	PCGExData::FPointIO* InEdges = nullptr;

	virtual bool ExecuteTask() override;
};
