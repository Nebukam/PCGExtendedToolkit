// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "PCGExRelaxEdgeIslands.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Edges")
class PCGEXTENDEDTOOLKIT_API UPCGExRelaxEdgeIslandsSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExRelaxEdgeIslandsSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(RelaxEdgeIslands, "Edges : Relax", "Relax point positions using edges connecting them.");
#endif

	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

private:
	friend class FPCGExRelaxEdgeIslandsElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExRelaxEdgeIslandsContext : public FPCGExEdgesProcessorContext
{
	friend class FPCGExRelaxEdgeIslandsElement;
};

class PCGEXTENDEDTOOLKIT_API FPCGExRelaxEdgeIslandsElement : public FPCGExEdgesProcessorElement
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
