﻿// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGraphProcessor.h"
#include "Data/PCGPointData.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExFindEdgesType.generated.h"

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExFindEdgesTypeSettings : public UPCGExGraphProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FindEdgesType, "Find Edges Type", "Find and writes the type of edge for each point and each socket.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	virtual int32 GetPreferredChunkSize() const override;
	virtual PCGEx::EIOInit GetPointOutputInitMode() const override;

public:

private:
	friend class FPCGExFindEdgesTypeElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExFindEdgesTypeContext : public FPCGExGraphProcessorContext
{
	friend class FPCGExFindEdgesTypeElement;
};

class PCGEXTENDEDTOOLKIT_API FPCGExFindEdgesTypeElement : public FPCGExGraphProcessorElement
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