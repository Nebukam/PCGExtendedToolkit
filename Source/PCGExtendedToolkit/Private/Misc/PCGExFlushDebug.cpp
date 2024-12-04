// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExFlushDebug.h"

#include "PCGGraph.h"
#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"
#define PCGEX_NAMESPACE Debug

#pragma region UPCGSettings interface

TArray<FPCGPinProperties> UPCGExDebugSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_ANY(PCGEx::SourcePointsLabel, "In.", Required, {})
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExDebugSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_ANY(PCGEx::SourcePointsLabel, "Out.", Required, {})
	return PinProperties;
}

FPCGElementPtr UPCGExDebugSettings::CreateElement() const { return MakeShared<FPCGExDebugElement>(); }

#pragma endregion

FPCGContext* FPCGExDebugElement::Initialize(
	const FPCGDataCollection& InputData,
	const TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExDebugContext* Context = new FPCGExDebugContext();

	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;

	return Context;
}

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

	FlushPersistentDebugLines(PCGEx::GetWorld(Context));
	FlushDebugStrings(PCGEx::GetWorld(Context));

#endif

	DisabledPassThroughData(Context);

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
