// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGPointData.h"
#include "Graph/PCGExGraphProcessor.h"
#include "PCGExEdgesToPaths.generated.h"

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExEdgesToPathsSettings : public UPCGExGraphProcessorSettings
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

	virtual int32 GetPreferredChunkSize() const override;
	virtual PCGEx::EIOInit GetPointOutputInitMode() const override;

public:
	/** Type of edge to draw.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(Bitmask, BitmaskEnum="EPCGExEdgeType"))
	EPCGExEdgeType EdgeType = EPCGExEdgeType::Complete;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bWriteTangents = true;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="bWriteTangents"))
	FName ArriveTangentAttribute = "ArriveTangent";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="bWriteTangents"))
	FName LeaveTangentAttribute = "LeaveTangent";
		
private:
	friend class FPCGExEdgesToPathsElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExEdgesToPathsContext : public FPCGExGraphProcessorContext
{
	friend class FPCGExEdgesToPathsElement;
};

class PCGEXTENDEDTOOLKIT_API FPCGExEdgesToPathsElement : public FPCGExGraphProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual void InitializeContext(
		FPCGExPointsProcessorContext* InContext,
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
