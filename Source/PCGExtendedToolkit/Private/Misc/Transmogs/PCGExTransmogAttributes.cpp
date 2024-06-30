// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Transmogs/PCGExTransmogAttributes.h"

#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExWriteTransmogAttributess"
#define PCGEX_NAMESPACE PCGExWriteTransmogAttributess


void UPCGExTransmogAttributesOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExTransmogAttributesOperation* TypedOther = Cast<UPCGExTransmogAttributesOperation>(Other))
	{
	}
}

bool UPCGExTransmogAttributesOperation::PrepareForData(const FPCGContext* InContext, PCGExData::FFacade* InPointDataCache)
{
	PrimaryDataCache = InPointDataCache;
	return true;
}

void UPCGExTransmogAttributesOperation::ProcessPoint(const FPCGPoint& Point, int64& Flags)
{
}

void UPCGExTransmogAttributesOperation::Cleanup()
{
	Super::Cleanup();
}

#if WITH_EDITOR
FString UPCGExTransmogAttributesProviderSettings::GetDisplayName() const { return TEXT(""); }
#endif

PCGEX_BITMASK_TRANSMUTE_CREATE_OPERATION(TransmogAttributes, {})

bool UPCGExTransmogAttributesFactory::Boot(FPCGContext* InContext)
{
	return true;
}

TArray<FPCGPinProperties> UPCGExTransmogAttributesProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_ANY(PCGExTransmogAttribute::SourceForwardSuccess, "TBD", Normal, {})
	PCGEX_PIN_ANY(PCGExTransmogAttribute::SourceForwardFail, "TBD", Normal, {})
	return PinProperties;
}

PCGEX_BITMASK_TRANSMUTE_CREATE_FACTORY(TransmogAttributes, {})


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
