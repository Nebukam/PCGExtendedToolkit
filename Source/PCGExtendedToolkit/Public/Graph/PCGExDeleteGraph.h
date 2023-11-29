// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExGraphProcessor.h"

#include "PCGExDeleteGraph.generated.h"

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExDeleteGraphSettings : public UPCGExGraphProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(DeleteGraph, "Delete Graph", "Delete data associated with given Graph Params.");
#endif
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	virtual PCGExIO::EInitMode GetPointOutputInitMode() const override;
	//~End UPCGSettings interface

	virtual int32 GetPreferredChunkSize() const override;

private:
	friend class FPCGExDeleteGraphElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExDeleteGraphContext : public FPCGExGraphProcessorContext
{
	friend class FPCGExDeleteGraphElement;
};


class PCGEXTENDEDTOOLKIT_API FPCGExDeleteGraphElement : public FPCGExGraphProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
