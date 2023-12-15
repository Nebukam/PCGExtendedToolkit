// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGExGraphParamsData.h"

#include "PCGExGraphProcessor.generated.h"

class UPCGExGraphParamsData;

/**
 * A Base node to process a set of point using GraphParams.
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExGraphProcessorSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(GraphProcessorSettings, "Graph Processor Settings", "TOOLTIP_TEXT");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorGraph; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings interface

	virtual FName GetMainInputLabel() const override;
	virtual FName GetMainOutputLabel() const override;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExGraphProcessorContext : public FPCGExPointsProcessorContext
{
	friend class UPCGExGraphProcessorSettings;

	virtual ~FPCGExGraphProcessorContext() override;

	bool bReadOnly = false;
	PCGExGraph::FGraphInputs Graphs;
	int32 MergedInputSocketsNum = 0;

	UPCGExGraphParamsData* CurrentGraph = nullptr;

	bool AdvanceGraph(bool bResetPointsIndex = false);
	bool AdvancePointsIOAndResetGraph();

	virtual void Reset() override;

	void SetCachedIndex(const int32 PointIndex, const int32 Index) const;
	int32 GetCachedIndex(const int32 PointIndex) const;

	TArray<PCGExGraph::FSocketInfos> SocketInfos;

	void PrepareCurrentGraphForPoints(const PCGExData::FPointIO& PointIO, const bool ReadOnly = true);
	void OutputGraphParams() { Graphs.OutputTo(this); }

	void OutputPointsAndGraphParams()
	{
		OutputPoints();
		OutputGraphParams();
	}

protected:
	PCGEx::TFAttributeReader<int32>* CachedIndexReader = nullptr;
	PCGEx::TFAttributeWriter<int32>* CachedIndexWriter = nullptr;
	int32 CurrentParamsIndex = -1;
};

class PCGEXTENDEDTOOLKIT_API FPCGExGraphProcessorElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual FPCGContext* InitializeContext(
		FPCGExPointsProcessorContext* InContext,
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) const override;
};
