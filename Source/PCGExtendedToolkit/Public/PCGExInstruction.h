// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExInstruction.generated.h"

struct FPCGExPointsProcessorContext;
/**
 * 
 */
UCLASS(Blueprintable, EditInlineNew)
class PCGEXTENDEDTOOLKIT_API UPCGExInstruction : public UObject
{
	GENERATED_BODY()

public:
	void BindContext(FPCGExPointsProcessorContext* InContext);
	virtual void UpdateUserFacingInfos();

protected:
	FPCGExPointsProcessorContext* Context;
	
};
