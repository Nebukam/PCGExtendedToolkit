// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGExGraphDefinition.h"

#include "PCGExCustomGraphProcessor.generated.h"

class UPCGExGraphDefinition;

#define PCGEX_GRAPH_MISSING_METADATA PCGE_LOG(Warning, GraphAndLog, FText::Format(FTEXT("Metadata missing on '{0}' graph."), FText::FromName(Context->CurrentGraph->GraphIdentifier)));

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExCustomGraphProcessorSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(CustomGraphProcessorSettings, "Graph Processor Settings", "TOOLTIP_TEXT");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExEditorSettings>()->NodeColorGraph; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual FName GetMainInputLabel() const override;
	virtual FName GetMainOutputLabel() const override;
	//~End UPCGExPointsProcessorSettings interface
};

struct PCGEXTENDEDTOOLKIT_API FPCGExCustomGraphProcessorContext : public FPCGExPointsProcessorContext
{
	friend class UPCGExCustomGraphProcessorSettings;

	virtual ~FPCGExCustomGraphProcessorContext() override;

	bool bReadOnly = false;
	PCGExGraph::FGraphInputs Graphs;
	int32 MergedInputSocketsNum = 0;

	UPCGExGraphDefinition* CurrentGraph = nullptr;

	bool AdvanceGraph(bool bResetPointsIndex = false);
	bool AdvancePointsIOAndResetGraph();

	void SetCachedIndex(const int32 PointIndex, const int32 Index) const;
	int32 GetCachedIndex(const int32 PointIndex) const;
	void WriteSocketInfos() const;

	TArray<PCGExGraph::FSocketInfos> SocketInfos;

	bool PrepareCurrentGraphForPoints(const PCGExData::FPointIO& PointIO, const bool ReadOnly = true);
	void OutputGraphParams() { Graphs.OutputTo(this); }

	void OutputPointsAndGraphParams()
	{
		OutputMainPoints();
		OutputGraphParams();
	}

	bool bValidCurrentGraph = false;
	FPCGExEdgeCrawlingSettings EdgeCrawlingSettings;
	uint8 CurrentGraphEdgeCrawlingTypes = static_cast<uint8>(EPCGExEdgeType::Complete);

protected:
	PCGEx::TFAttributeReader<int32>* CachedIndexReader = nullptr;
	PCGEx::TFAttributeWriter<int32>* CachedIndexWriter = nullptr;
	int32 CurrentParamsIndex = -1;
};

class PCGEXTENDEDTOOLKIT_API FPCGExCustomGraphProcessorElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

	virtual void DisabledPassThroughData(FPCGContext* Context) const override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual FPCGContext* InitializeContext(
		FPCGExPointsProcessorContext* InContext,
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) const override;
};
