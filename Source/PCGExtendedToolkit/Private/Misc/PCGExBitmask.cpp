// Copyright 2025 Timothé Lapetite and contributors
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
	PCGEX_PIN_PARAM(FName("Bitmask"), TEXT("Bitmask."), Required, {})
	return PinProperties;
}

FPCGElementPtr UPCGExBitmaskSettings::CreateElement() const { return MakeShared<FPCGExBitmaskElement>(); }

#pragma endregion

bool FPCGExBitmaskElement::ExecuteInternal(FPCGContext* InContext) const
{
	PCGEX_CONTEXT()
	PCGEX_SETTINGS(Bitmask)

	const int64 Bitmask = Settings->Bitmask.Get();

	UPCGParamData* BitmaskData = Context->ManagedObjects->New<UPCGParamData>();
	BitmaskData->Metadata->CreateAttribute<int64>(FName("Bitmask"), Bitmask, false, true);
	BitmaskData->Metadata->AddEntry();

	FPCGTaggedData& StagedData = Context->StageOutput(BitmaskData, true);
	StagedData.Pin = FName("Bitmask");
	
	Context->Done();
	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
