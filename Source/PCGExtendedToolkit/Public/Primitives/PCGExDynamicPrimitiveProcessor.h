// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "PCGExDynamicPrimitiveProcessor.generated.h"

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExDynamicPrimitiveProcessorSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(DynamicPrimitiveProcessor, "DynamicPrimitiveProcessor", "TOOLTIP");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorPrimitives; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	//~End UPCGSettings interface
};

struct PCGEXTENDEDTOOLKIT_API FPCGExDynamicPrimitiveProcessorContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExDynamicPrimitiveProcessorElement;
};

class PCGEXTENDEDTOOLKIT_API FPCGExDynamicPrimitiveProcessorElement : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;
};
