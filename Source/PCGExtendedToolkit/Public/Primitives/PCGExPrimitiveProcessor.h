// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "PCGExPrimitiveProcessor.generated.h"

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExPrimitiveProcessorSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PrimitiveProcessor, "PrimitiveProcessor", "TOOLTIP");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorPrimitives; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	//~End UPCGSettings

public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPrimitiveProcessorContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExPrimitiveProcessorElement;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPrimitiveProcessorElement : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;
};
