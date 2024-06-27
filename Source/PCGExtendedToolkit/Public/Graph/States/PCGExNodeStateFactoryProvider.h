// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"

#include "PCGExNodeStateFactoryProvider.generated.h"

#define PCGEX_CREATE_NodeState_FACTORY(_NAME, _EXTRA_FACTORY, _EXTRA_OPERATION) \
UPCGExParamFactoryBase* UPCGExNodeState##_NAME##ProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const{\
	UPCGExNodeStateFactory##_NAME* NewFactory = NewObject<UPCGExNodeStateFactory##_NAME>(); \
	NewFactory->Descriptor = Descriptor; _EXTRA_FACTORY \
	return Super::CreateFactory(InContext, NewFactory); } \
UPCGExNodeStateOperation* UPCGExNodeStateFactory##_NAME::CreateOperation() const{\
	UPCGExNodeState##_NAME* NewOperation = NewObject<UPCGExNodeState##_NAME>();\
	NewOperation->Descriptor = Descriptor; NewOperation->BaseDescriptor = &NewOperation->Descriptor; _EXTRA_OPERATION return NewOperation;}

class UPCGExNodeStateOperation;

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExNodeStateFactoryBase : public UPCGExParamFactoryBase
{
	GENERATED_BODY()

public:
	virtual PCGExFactories::EType GetFactoryType() const override;
	virtual UPCGExNodeStateOperation* CreateOperation() const;
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExNodeStateFactoryProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(NodeFilter, "NodeState Definition", "Creates a single NodeState to look for a nerbay connection.")
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorClusterState; }
#endif
	//~End UPCGSettings

	virtual FName GetMainOutputLabel() const override;
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

};
