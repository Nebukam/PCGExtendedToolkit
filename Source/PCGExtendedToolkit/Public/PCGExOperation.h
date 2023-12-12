// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGExPointIO.h"
#include "UObject/Object.h"
#include "PCGExOperation.generated.h"

struct FPCGExPointsProcessorContext;
/**
 * 
 */
UCLASS(Blueprintable, EditInlineNew)
class PCGEXTENDEDTOOLKIT_API UPCGExOperation : public UObject
{
	GENERATED_BODY()

public:
	void BindContext(FPCGExPointsProcessorContext* InContext);
	virtual void UpdateUserFacingInfos();
	virtual void Cleanup();

	virtual void BeginDestroy() override;

protected:
	FPCGExPointsProcessorContext* Context = nullptr;
};
