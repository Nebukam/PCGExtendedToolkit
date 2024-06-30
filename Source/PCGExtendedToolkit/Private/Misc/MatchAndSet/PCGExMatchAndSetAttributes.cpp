// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/MatchAndSet/PCGExMatchAndSetAttributes.h"

#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExWriteMatchAndSetAttributess"
#define PCGEX_NAMESPACE PCGExWriteMatchAndSetAttributess


void UPCGExMatchAndSetAttributesOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExMatchAndSetAttributesOperation* TypedOther = Cast<UPCGExMatchAndSetAttributesOperation>(Other))
	{
	}
}

bool UPCGExMatchAndSetAttributesOperation::PrepareForData(const FPCGContext* InContext, PCGExData::FFacade* InPointDataCache)
{
	if (!Super::PrepareForData(InContext, InPointDataCache)) { return false; }
	return true;
}

void UPCGExMatchAndSetAttributesOperation::OnMatchSuccess(int32 Index, const FPCGPoint& Point)
{
	Super::OnMatchSuccess(Index, Point);
}

void UPCGExMatchAndSetAttributesOperation::OnMatchFail(int32 Index, const FPCGPoint& Point)
{
	Super::OnMatchFail(Index, Point);
}

void UPCGExMatchAndSetAttributesOperation::Cleanup()
{
	Super::Cleanup();
}

#if WITH_EDITOR
FString UPCGExMatchAndSetAttributesProviderSettings::GetDisplayName() const { return TEXT(""); }
#endif

PCGEX_BITMASK_TRANSMUTE_CREATE_OPERATION(MatchAndSetAttributes, {})

bool UPCGExMatchAndSetAttributesFactory::Boot(FPCGContext* InContext)
{
	return true;
}

TArray<FPCGPinProperties> UPCGExMatchAndSetAttributesProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_ANY(PCGExMatchAndSetAttribute::SourceForwardSuccess, "TBD", Normal, {})
	PCGEX_PIN_ANY(PCGExMatchAndSetAttribute::SourceForwardFail, "TBD", Normal, {})
	return PinProperties;
}

PCGEX_BITMASK_TRANSMUTE_CREATE_FACTORY(MatchAndSetAttributes, {})


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
