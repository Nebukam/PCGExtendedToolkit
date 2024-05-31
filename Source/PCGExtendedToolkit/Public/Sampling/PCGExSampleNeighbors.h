// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Geometry/PCGExGeo.h"
#include "Graph/PCGExEdgesProcessor.h"

#include "PCGExSampleNeighbors.generated.h"

class UPCGExNeighborSampleOperation;

namespace PCGExContours
{
	struct PCGEXTENDEDTOOLKIT_API FCandidate
	{
		explicit FCandidate(const int32 InNodeIndex)
			: NodeIndex(InNodeIndex)
		{
		}

		int32 NodeIndex;
		double Distance = TNumericLimits<double>::Max();
		double Dot = -1;
	};
}

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Edges")
class PCGEXTENDEDTOOLKIT_API UPCGExSampleNeighborsSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExSampleNeighborsSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SampleNeighbors, "Sample : Neighbors", "Sample graph node' neighbors values.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorSampler; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;

private:
	friend class FPCGExSampleNeighborsElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExSampleNeighborsContext : public FPCGExEdgesProcessorContext
{
	friend class FPCGExSampleNeighborsElement;
	virtual ~FPCGExSampleNeighborsContext() override;

	TArray<UPCGExNeighborSampleOperation*> SamplingOperations;
	TArray<UPCGExNeighborSampleOperation*> PointPointOperations;
	TArray<UPCGExNeighborSampleOperation*> PointEdgeOperations;

	FPCGExBlendingSettings PointPointBlendingSettings;
	FPCGExBlendingSettings PointEdgeBlendingSettings;

	PCGExDataBlending::FMetadataBlender* BlenderFromPoints = nullptr;
	PCGExDataBlending::FMetadataBlender* BlenderFromEdges = nullptr;

	bool PrepareSettings(
		FPCGExBlendingSettings& OutSettings,
		TArray<UPCGExNeighborSampleOperation*>& OutOperations,
		const PCGExData::FPointIO& FromPointIO,
		EPCGExGraphValueSource Source) const;
};

class PCGEXTENDEDTOOLKIT_API FPCGExSampleNeighborsElement : public FPCGExEdgesProcessorElement
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

class PCGEXTENDEDTOOLKIT_API FPCGExSampleNeighborTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExSampleNeighborTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO)
	{
	}

	virtual bool ExecuteTask() override;
};

