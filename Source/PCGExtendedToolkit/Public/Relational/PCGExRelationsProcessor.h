// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGExPointsProcessor.h"
#include "PCGExRelationsHelpers.h"
#include "PCGSettings.h"
#include "PCGExRelationsProcessor.generated.h"

class UPCGExRelationsParamsData;

namespace PCGExRelational
{
	extern const FName SourceRelationalParamsLabel;
}

/**
 * A Base node to process a set of point using RelationalParams.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExRelationsProcessorSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(RelationsProcessorSettings, "Relations Processor Settings", "TOOLTIP_TEXT");
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	//~End UPCGSettings interface

};

struct PCGEXTENDEDTOOLKIT_API FPCGExRelationsProcessorContext : public FPCGExPointsProcessorContext
{

public:
	PCGExRelational::FParamsInputs Params;

	int32 GetCurrentParamsIndex() const { return CurrentParamsIndex; };
	UPCGExRelationsParamsData* CurrentParams = nullptr;

	bool AdvanceParams(bool bResetPointsIndex = false);
	bool AdvancePointsIO(bool bResetParamsIndex = false);

	virtual void Reset() override;
	virtual bool IsValid() override;

protected:
	int32 CurrentParamsIndex = -1;
};

class PCGEXTENDEDTOOLKIT_API FPCGExRelationsProcessorElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual PCGEx::EIOInit GetPointOutputInitMode() const { return PCGEx::EIOInit::DuplicateInput; }
	virtual void InitializeContext(
		FPCGExPointsProcessorContext* InContext,
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) const override;
};
