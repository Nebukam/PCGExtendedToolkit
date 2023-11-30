// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExDebugManager.h"

#include "IPCGExDebug.h"
#include "PCGGraph.h"
#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

UPCGExDebugSettings::UPCGExDebugSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TArray<FPCGPinProperties> UPCGExDebugSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Empty;
	return Empty;
}

TArray<FPCGPinProperties> UPCGExDebugSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Empty;
	return Empty;
}

FPCGElementPtr UPCGExDebugSettings::CreateElement() const { return MakeShared<FPCGExDebugElement>(); }


#pragma endregion

FPCGContext* FPCGExDebugElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExDebugContext* Context = new FPCGExDebugContext();

	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;

	return Context;
}

bool FPCGExDebugElement::ExecuteInternal(FPCGContext* Context) const
{	
#if WITH_EDITOR

	const int32 DebugNodeCount = PCGExDebug::GetActiveDebugNodeCount(Context);

	const UPCGExDebugSettings* Settings = Context->GetInputSettings<UPCGExDebugSettings>();
	check(Settings);

	UPCGExDebugSettings* MutableSettings = const_cast<UPCGExDebugSettings*>(Settings);
	if (MutableSettings->DebugNodeCount != DebugNodeCount && DebugNodeCount == 0)
	{
		// This ensure we flush undesirable (deprecated) debug lines
		MutableSettings->ResetPing(Context);
	}
	else
	{
		MutableSettings->DebugNodeCount = DebugNodeCount;
	}

#endif

	return true;
}

#undef LOCTEXT_NAMESPACE
