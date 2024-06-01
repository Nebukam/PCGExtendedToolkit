// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGExDataStateFactoryProvider.h"

#include "PCGExCreateNodeState.generated.h"

/** Outputs a single GraphParam to be consumed by other nodes */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExCreateNodeStateSettings : public UPCGExStateFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		NodeState, "Node State Definition", "Creates a node state configuration.",
		StateName)
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorClusterState; }
#endif
	virtual FName GetMainOutputLabel() const override;
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	//~End UPCGSettings

public:
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* Context, UPCGExParamFactoryBase* InFactory) const override;
};
