// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "PCGExConsolidateEdgeIslands.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Edges")
class PCGEXTENDEDTOOLKIT_API UPCGExConsolidateEdgeIslandsSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExConsolidateEdgeIslandsSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ConsolidateEdgeIslands, "Edges : Consolidate Islands", "Connects isolated edge islands by their closest vertices.");
#endif

	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;
	virtual bool GetCacheAllMeshes() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

private:
	friend class FPCGExConsolidateEdgeIslandsElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExConsolidateEdgeIslandsContext : public FPCGExEdgesProcessorContext
{
	friend class FPCGExConsolidateEdgeIslandsElement;

	virtual ~FPCGExConsolidateEdgeIslandsContext() override;

	PCGExData::FPointIO* ConsolidatedEdges = nullptr;
	TSet<PCGExMesh::FMesh*> VisitedMeshes;
	
};

class PCGEXTENDEDTOOLKIT_API FPCGExConsolidateEdgeIslandsElement : public FPCGExEdgesProcessorElement
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

// Define the background task class
class PCGEXTENDEDTOOLKIT_API FBridgeMeshesTask : public FPCGExNonAbandonableTask
{
public:
	FBridgeMeshesTask(
		FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO, int32 InOtherMeshIndex) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
	OtherMeshIndex(InOtherMeshIndex)
	{
	}

	int32 OtherMeshIndex = -1;
	
	virtual bool ExecuteTask() override;

};
