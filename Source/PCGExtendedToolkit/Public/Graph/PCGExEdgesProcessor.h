// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExMesh.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGExData.h"

#include "PCGExEdgesProcessor.generated.h"

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
	
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual FName GetMainInputLabel() const override;
	virtual FName GetMainOutputLabel() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const;
	virtual bool GetMainAcceptMultipleData() const override;
	
	//~End UPCGSettings interface

	virtual bool GetCacheAllMeshes() const;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExEdgesProcessorContext : public FPCGExPointsProcessorContext
{
	friend class UPCGExEdgesProcessorSettings;
	friend class FPCGExEdgesProcessorElement;

	virtual ~FPCGExEdgesProcessorContext() override;

	PCGExData::FPointIOGroup* Edges = nullptr;
	PCGExData::FKPointIOMarkedBindings<int32>* BoundEdges = nullptr;

	PCGExData::FPointIO* CurrentEdges = nullptr;
	bool AdvanceAndBindPointsIO();
	bool AdvanceEdges(); // Advance edges within current points

	bool bCacheAllMeshes = false;
	PCGExMesh::FMesh* CurrentMesh = nullptr;
	TArray<PCGExMesh::FMesh*> Meshes;

	void OutputPointsAndEdges();

	template <class InitializeFunc, class LoopBodyFunc>
	bool ProcessCurrentEdges(InitializeFunc&& Initialize, LoopBodyFunc&& LoopBody, bool bForceSync = false) { return Process(Initialize, LoopBody, CurrentEdges->GetNum(), bForceSync); }

	template <class LoopBodyFunc>
	bool ProcessCurrentEdges(LoopBodyFunc&& LoopBody, bool bForceSync = false) { return Process(LoopBody, CurrentEdges->GetNum(), bForceSync); }

	template <class InitializeFunc, class LoopBodyFunc>
	bool ProcessCurrentMesh(InitializeFunc&& Initialize, LoopBodyFunc&& LoopBody, bool bForceSync = false) { return Process(Initialize, LoopBody, CurrentMesh->Vertices.Num(), bForceSync); }

	template <class LoopBodyFunc>
	bool ProcessCurrentMesh(LoopBodyFunc&& LoopBody, bool bForceSync = false) { return Process(LoopBody, CurrentMesh->Vertices.Num(), bForceSync); }

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
