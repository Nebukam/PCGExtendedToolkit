// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"

#include "Data/PCGExGraphDefinition.h"

#include "PCGExProbeFactoryProvider.generated.h"

#define PCGEX_CREATE_PROBE_FACTORY(_NAME, _EXTRA_FACTORY, _EXTRA_OPERATION) \
UPCGExParamFactoryBase* UPCGExProbe##_NAME##ProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const{\
	UPCGExProbeFactory##_NAME* NewFactory = NewObject<UPCGExProbeFactory##_NAME>(); \
	NewFactory->Descriptor = Descriptor; _EXTRA_FACTORY \
	return Super::CreateFactory(InContext, NewFactory); } \
UPCGExProbeOperation* UPCGExProbeFactory##_NAME::CreateOperation() const{\
	UPCGExProbe##_NAME* NewOperation = NewObject<UPCGExProbe##_NAME>();\
	NewOperation->Descriptor = Descriptor; NewOperation->BaseDescriptor = &NewOperation->Descriptor; _EXTRA_OPERATION return NewOperation;}

class UPCGExProbeOperation;

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExProbeFactoryBase : public UPCGExParamFactoryBase
{
	GENERATED_BODY()

public:
	virtual PCGExFactories::EType GetFactoryType() const override;
	virtual UPCGExProbeOperation* CreateOperation() const;
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExProbeFactoryProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(NodeFilter, "Probe Definition", "Creates a single probe to look for a nerbay connection.")
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorProbe; }
#endif
	//~End UPCGSettings

	virtual FName GetMainOutputLabel() const override;
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

};
