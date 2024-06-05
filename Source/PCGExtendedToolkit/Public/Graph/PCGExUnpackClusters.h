// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "PCGExUnpackClusters.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Edges")
class PCGEXTENDEDTOOLKIT_API UPCGExUnpackClustersSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExUnpackClustersSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(UnpackClusters, "Graph : Unpack Clusters", "Restores vtx/edge clusters from packed dataset.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExEditorSettings>()->NodeColorGraph; }
#endif
	virtual FName GetMainInputLabel() const override;
	virtual FName GetMainOutputLabel() const override;
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

	/** Flatten unpacked metadata \n Depending on your setup this is a tradeoff between memory and speed.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bFlatten = false;

private:
	friend class FPCGExUnpackClustersElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExUnpackClustersContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExUnpackClustersElement;

	virtual ~FPCGExUnpackClustersContext() override;

	PCGExData::FPointIOCollection* OutPoints = nullptr;
	PCGExData::FPointIOCollection* OutEdges = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExUnpackClustersElement : public FPCGExPointsProcessorElementBase
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

class PCGEXTENDEDTOOLKIT_API FPCGExUnpackClusterTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExUnpackClusterTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO)
	{
	}

	virtual bool ExecuteTask() override;
};
