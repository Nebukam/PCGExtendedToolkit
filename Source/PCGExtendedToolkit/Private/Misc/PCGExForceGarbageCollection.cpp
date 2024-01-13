// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExForceGarbageCollection.h"

#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

UPCGExForceGarbageCollectionSettings::UPCGExForceGarbageCollectionSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TArray<FPCGPinProperties> UPCGExForceGarbageCollectionSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertySource = PinProperties.Emplace_GetRef("Any", EPCGDataType::Any, true, true);

#if WITH_EDITOR
	PinPropertySource.Tooltip = FTEXT("Anything really.");
#endif

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExForceGarbageCollectionSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	return PinProperties;
}

FPCGElementPtr UPCGExForceGarbageCollectionSettings::CreateElement() const { return MakeShared<FPCGExForceGarbageCollectionElement>(); }

bool FPCGExForceGarbageCollectionElement::ExecuteInternal(FPCGContext* Context) const
{
	GEngine->ForceGarbageCollection();
	return true;
}

#undef LOCTEXT_NAMESPACE
