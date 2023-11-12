// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExRelationsProcessor.h"
#include "Data/PCGPointData.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExMarkMutualRelations.generated.h"

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Relational")
class PCGEXTENDEDTOOLKIT_API UPCGExMarkMutualRelationsSettings : public UPCGExRelationsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(MarkMutualRelations, "Mark Mutual Relations", "Marks which relations are mutual in the socket.Y slot");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	virtual int32 GetPreferredChunkSize() const override;

public:


private:
	friend class FPCGExMarkMutualRelationsElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExMarkMutualRelationsContext : public FPCGExRelationsProcessorContext
{
	friend class FPCGExMarkMutualRelationsElement;
};


class PCGEXTENDEDTOOLKIT_API FPCGExMarkMutualRelationsElement : public FPCGExRelationsProcessorElement
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
	virtual void DrawRelationsDebug(FPCGExMarkMutualRelationsContext* Context) const;
};
