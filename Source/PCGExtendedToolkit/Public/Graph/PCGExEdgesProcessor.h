// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGExData.h"
#include "Data/PCGExGraphParamsData.h"

#include "PCGExEdgesProcessor.generated.h"

namespace PCGExGraph
{
	struct PCGEXTENDEDTOOLKIT_API FVertex
	{
		int32 Index;
		TArray<int32> Neighbors;
	};

	struct PCGEXTENDEDTOOLKIT_API FMesh
	{
		int32 MeshID = -1;
		TArray<FVertex> Vertices;    //Mesh vertices
		TMap<uint64, int32> EdgeMap; //Edge hash to edge index
	};
}

/**
 * A Base node to process a set of point using GraphParams.
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExEdgesProcessorSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(EdgesProcessorSettings, "Edges Processor Settings", "TOOLTIP_TEXT");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorEdge; }
#endif
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const;
	virtual bool GetMainAcceptMultipleData() const override;

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings interface
};

struct PCGEXTENDEDTOOLKIT_API FPCGExEdgesProcessorContext : public FPCGExPointsProcessorContext
{
	friend class UPCGExEdgesProcessorSettings;

	~FPCGExEdgesProcessorContext();

	PCGExData::FPointIOGroup* Edges = nullptr;
	PCGExData::FKPointIOMarkedBindings<int32>* BoundEdges = nullptr;

	PCGExData::FPointIO* CurrentEdges = nullptr;
	bool AdvanceAndBindPointsIO();
	bool AdvanceEdges();

	TArray<PCGExGraph::FMesh> Meshes;

protected:
	int32 CurrentEdgesIndex = -1;
};

class PCGEXTENDEDTOOLKIT_API FPCGExEdgesProcessorElement : public FPCGExPointsProcessorElementBase
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
