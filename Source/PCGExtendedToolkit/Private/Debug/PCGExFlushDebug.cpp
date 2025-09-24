// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Debug/PCGExFlushDebug.h"

#include "PCGGraph.h"
#include "PCGPin.h"

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

bool FPCGExDebugElement::ExecuteInternal(FPCGContext* InContext) const
{
	PCGEX_CONTEXT_AND_SETTINGS(Debug)

#if WITH_EDITOR

	if (!Settings->bPCGExDebug)
	{
		DisabledPassThroughData(Context);
		return true;
	}

	if (Context->bWait)
	{
		Context->bWait = false;
		return false;
	}

	FlushPersistentDebugLines(Context->GetWorld());
	FlushDebugStrings(Context->GetWorld());

#endif

	DisabledPassThroughData(Context);

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
