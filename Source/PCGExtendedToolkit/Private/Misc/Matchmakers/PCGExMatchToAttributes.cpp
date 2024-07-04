// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Matchmakers/PCGExMatchToAttributes.h"

#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExWriteMatchToAttributess"
#define PCGEX_NAMESPACE PCGExWriteMatchToAttributess


void UPCGExMatchToAttributesOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExMatchToAttributesOperation* TypedOther = Cast<UPCGExMatchToAttributesOperation>(Other))
	{
	}
}

bool UPCGExMatchToAttributesOperation::PrepareForData(const FPCGContext* InContext, PCGExData::FFacade* InPointDataFacade)
{
	if (!Super::PrepareForData(InContext, InPointDataFacade)) { return false; }
	return true;
}

void UPCGExMatchToAttributesOperation::OnMatchSuccess(int32 Index, const FPCGPoint& Point)
{
	Super::OnMatchSuccess(Index, Point);
}

void UPCGExMatchToAttributesOperation::OnMatchFail(int32 Index, const FPCGPoint& Point)
{
	Super::OnMatchFail(Index, Point);
}

void UPCGExMatchToAttributesOperation::Cleanup()
{
	Super::Cleanup();
}

#if WITH_EDITOR
FString UPCGExMatchToAttributesProviderSettings::GetDisplayName() const { return TEXT(""); }
#endif

PCGEX_BITMASK_TRANSMUTE_CREATE_OPERATION(MatchToAttributes, {})

bool UPCGExMatchToAttributesFactory::Boot(FPCGContext* InContext)
{
	return true;
}

TArray<FPCGPinProperties> UPCGExMatchToAttributesProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_ANY(PCGExMatchToAttribute::SourceForwardSuccess, "TBD", Normal, {})
	PCGEX_PIN_ANY(PCGExMatchToAttribute::SourceForwardFail, "TBD", Normal, {})
	return PinProperties;
}

PCGEX_BITMASK_TRANSMUTE_CREATE_FACTORY(MatchToAttributes, {})


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
