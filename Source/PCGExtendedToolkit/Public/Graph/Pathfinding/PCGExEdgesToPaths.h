// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathfindingProcessor.h"
#include "PCGExEdgesToPaths.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExEdgesToPathsSettings : public UPCGExPathfindingProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(EdgesToPaths, "Edges To Paths", "Converts graph edges to paths-like data that can be used to generate splines.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	virtual int32 GetPreferredChunkSize() const override;
	virtual PCGExIO::EInitMode GetPointOutputInitMode() const override;
	virtual bool GetRequiresSeeds() const override;
	virtual bool GetRequiresGoals() const override;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExEdgeType"))
	uint8 EdgeType = static_cast<uint8>(EPCGExEdgeType::Complete);

private:
	friend class FPCGExEdgesToPathsElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExEdgesToPathsContext : public FPCGExPathfindingProcessorContext
{
	friend class FPCGExEdgesToPathsElement;

public:
	EPCGExEdgeType EdgeType;
	TSet<uint64> UniqueEdges;
	TArray<PCGExGraph::FUnsignedEdge> Edges;

	mutable FRWLock EdgeLock;
};

class PCGEXTENDEDTOOLKIT_API FPCGExEdgesToPathsElement : public FPCGExPathfindingProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
