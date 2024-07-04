// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExBitmask.h"

#include "PCGGraph.h"
#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"
#define PCGEX_NAMESPACE Bitmask

#pragma region UPCGSettings interface

TArray<FPCGPinProperties> UPCGExBitmaskSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExBitmaskSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPointsOutput = PinProperties.Emplace_GetRef(FName("Bitmask"), EPCGDataType::Param);

#if WITH_EDITOR
	PinPointsOutput.Tooltip = FTEXT("Bitmask.");
#endif

	return PinProperties;
}

FPCGElementPtr UPCGExBitmaskSettings::CreateElement() const { return MakeShared<FPCGExBitmaskElement>(); }

#pragma endregion

FPCGContext* FPCGExBitmaskElement::Initialize(
	const FPCGDataCollection& InputData,
	const TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGContext* Context = new FPCGContext();

	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;

	return Context;
}

bool FPCGExBitmaskElement::ExecuteInternal(FPCGContext* Context) const
{
	PCGEX_SETTINGS(Bitmask)

	UPCGParamData* Bitmask = NewObject<UPCGParamData>();
	Bitmask->Metadata->CreateAttribute(FName("Bitmask"), Settings->Bitmask.Get(), false, true);
	Bitmask->Metadata->AddEntry();
	FPCGTaggedData& OutData = Context->OutputData.TaggedData.Emplace_GetRef();
	OutData.Pin = FName("Bitmask");
	OutData.Data = Bitmask;

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
