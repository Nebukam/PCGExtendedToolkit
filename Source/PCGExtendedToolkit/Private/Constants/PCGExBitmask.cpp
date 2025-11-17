// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Constants/PCGExBitmask.h"

#include "PCGExHelpers.h"
#include "PCGGraph.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "Details/PCGExVersion.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"
#define PCGEX_NAMESPACE Bitmask

#if WITH_EDITOR
void UPCGExBitmaskSettings::ApplyDeprecation(UPCGNode* InOutNode)
{
	PCGEX_IF_DATA_VERSION(1, 71, 2)
	{
		Bitmask.ApplyDeprecation();
	}

	PCGEX_UPDATE_DATA_VERSION
	Super::ApplyDeprecation(InOutNode);
}
#endif

TArray<FPCGPinProperties> UPCGExBitmaskSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExBitmaskSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_PARAM(FName("Bitmask"), TEXT("Bitmask."), Required)
	return PinProperties;
}

FPCGElementPtr UPCGExBitmaskSettings::CreateElement() const { return MakeShared<FPCGExBitmaskElement>(); }


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
