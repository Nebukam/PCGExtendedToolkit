// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExRelationsProcessor.h"
#include "Data/PCGPointData.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExDrawRelations.generated.h"

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Relational")
class PCGEXTENDEDTOOLKIT_API UPCGExDrawRelationsSettings : public UPCGExRelationsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(DrawRelations, "Draw Relations", "Draw debug relations. Warning: this node will clear persistent debug lines!");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Debug; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	virtual PCGEx::EIOInit GetPointOutputInitMode() const override;

private:
	friend class FPCGExDrawRelationsElement;
};

class PCGEXTENDEDTOOLKIT_API FPCGExDrawRelationsElement : public FPCGExRelationsProcessorElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
