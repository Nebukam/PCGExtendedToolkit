// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "PCGExPruneEdges.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Edges")
class PCGEXTENDEDTOOLKIT_API UPCGExPruneEdgesSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExPruneEdgesSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PruneEdges, "Edges : Prune", "Remove edges according using a custom processor.");
#endif

	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

private:
	friend class FPCGExPruneEdgesElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPruneEdgesContext : public FPCGExEdgesProcessorContext
{
	friend class FPCGExPruneEdgesElement;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPruneEdgesElement : public FPCGExEdgesProcessorElement
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
