// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/Debug/PCGExFlushDebug.h"

#include "PCGGraph.h"
#include "PCGPin.h"
#include "DrawDebugHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"
#define PCGEX_NAMESPACE Debug

#pragma region UPCGSettings interface

TArray<FPCGPinProperties> UPCGExDebugSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_ANY(PCGPinConstants::DefaultInputLabel, "In.", Required)
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExDebugSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_ANY(PCGPinConstants::DefaultInputLabel, "Out.", Required)
	return PinProperties;
}

FPCGElementPtr UPCGExDebugSettings::CreateElement() const { return MakeShared<FPCGExDebugElement>(); }

#pragma endregion

bool FPCGExDebugElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	PCGEX_SETTINGS_C(InContext, Debug)

#if WITH_EDITOR

	if (Settings->bPCGExDebug)
	{
		FlushPersistentDebugLines(InContext->GetWorld());
		FlushDebugStrings(InContext->GetWorld());
	}

#endif

	DisabledPassThroughData(InContext);

	InContext->Done();
	return InContext->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
