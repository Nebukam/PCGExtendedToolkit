// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExBitmaskMerge.h"

#include "PCGPin.h"
#include "Misc/PCGExBitmask.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"
#define PCGEX_NAMESPACE BitmaskMerge

#pragma region UPCGSettings interface

TArray<FPCGPinProperties> UPCGExBitmaskMergeSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_PARAMS(FName("Bitmasks"), TEXT("Bitmask."), Required, {})
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExBitmaskMergeSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_PARAM(FName("Bitmask"), TEXT("Bitmask."), Required, {})
	return PinProperties;
}

FPCGElementPtr UPCGExBitmaskMergeSettings::CreateElement() const { return MakeShared<FPCGExBitmaskMergeElement>(); }

#pragma endregion

FPCGContext* FPCGExBitmaskMergeElement::Initialize(
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

bool FPCGExBitmaskMergeElement::ExecuteInternal(FPCGContext* Context) const
{
	PCGEX_SETTINGS(BitmaskMerge)

	TArray<FPCGTaggedData> InputParams = Context->InputData.GetInputsByPin(FName("Bitmasks"));

	bool bInitialized = false;
	int64 OutputMask = 0;

	for (FPCGTaggedData& TaggedData : InputParams)
	{
		const UPCGParamData* ParamData = Cast<UPCGParamData>(TaggedData.Data);
		if (!ParamData) { continue; }

		const UPCGMetadata* Metadata = ParamData->Metadata;
		if (!Metadata) { continue; }

		const TSharedPtr<PCGEx::FAttributesInfos> Infos = PCGEx::FAttributesInfos::Get(Metadata);

		for (int i = 0; i < Infos->Attributes.Num(); ++i)
		{
			if (Infos->Identities[i].UnderlyingType != EPCGMetadataTypes::Integer64) { continue; }

			const int64 InputMask = static_cast<FPCGMetadataAttribute<int64>*>(Infos->Attributes[i])->GetValue(PCGInvalidEntryKey);

			if (!bInitialized)
			{
				bInitialized = true;
				OutputMask = InputMask;
				continue;
			}

			PCGExBitmask::Do(Settings->Operation, OutputMask, InputMask);
		}
	}

	UPCGParamData* Bitmask = NewObject<UPCGParamData>();
	Bitmask->Metadata->CreateAttribute<int64>(FName("Bitmask"), OutputMask, false, true);
	Bitmask->Metadata->AddEntry();
	FPCGTaggedData& OutData = Context->OutputData.TaggedData.Emplace_GetRef();
	OutData.Pin = FName("Bitmask");
	OutData.Data = Bitmask;

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
