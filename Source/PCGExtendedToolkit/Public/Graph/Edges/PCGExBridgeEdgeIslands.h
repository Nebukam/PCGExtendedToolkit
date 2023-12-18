// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "PCGExBridgeEdgeIslands.generated.h"

UENUM(BlueprintType)
enum class EPCGExBridgeIslandMethod : uint8
{
	LeastEdges UMETA(DisplayName = "Least Edges", ToolTip="Ensure all islands are connected using the least possible number of bridges."),
	MostEdges UMETA(DisplayName = "Most Edges", ToolTip="Each island will have a bridge to every other island"),
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Edges")
class PCGEXTENDEDTOOLKIT_API UPCGExBridgeEdgeIslandsSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExBridgeEdgeIslandsSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BridgeEdgeIslands, "Edges : Bridge Islands", "Connects isolated edge islands by their closest vertices.");
#endif

	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;
	virtual bool GetCacheAllMeshes() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExBridgeIslandMethod BridgeMethod = EPCGExBridgeIslandMethod::LeastEdges;

private:
	friend class FPCGExBridgeEdgeIslandsElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExBridgeEdgeIslandsContext : public FPCGExEdgesProcessorContext
{
	friend class FPCGExBridgeEdgeIslandsElement;

	virtual ~FPCGExBridgeEdgeIslandsContext() override;

	EPCGExBridgeIslandMethod BridgeMethod;

	PCGExData::FPointIO* ConsolidatedEdges = nullptr;
	TSet<PCGExMesh::FMesh*> VisitedMeshes;
};

class PCGEXTENDEDTOOLKIT_API FPCGExBridgeEdgeIslandsElement : public FPCGExEdgesProcessorElement
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
