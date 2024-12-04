// Copyright 2024 Edward Boucher and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Constants/PCGExConstants.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGExContext.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"

#if WITH_EDITOR
FName UPCGExConstantsSettings::GetEnumName() const
{
	if (const UEnum* EnumPtr = StaticEnum<EPCGExConstantListID>())
	{
		return FName(EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(ConstantList)).ToString());
	}
	return NAME_None;
}

TArray<FPCGPreConfiguredSettingsInfo> UPCGExConstantsSettings::GetPreconfiguredInfo() const
{
	return PCGMetadataElementCommon::FillPreconfiguredSettingsInfoFromEnum<EPCGExConstantListID>();
}
#endif

void UPCGExConstantsSettings::ApplyPreconfiguredSettings(const FPCGPreConfiguredSettingsInfo& PreconfigureInfo)
{
	if (const UEnum* EnumPtr = StaticEnum<EPCGExConstantListID>())
	{
		if (EnumPtr->IsValidEnumValue(PreconfigureInfo.PreconfiguredIndex))
		{
			ConstantList = static_cast<EPCGExConstantListID>(PreconfigureInfo.PreconfiguredIndex);
		}
	}
}

TArray<FPCGPinProperties> UPCGExConstantsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

#define PCGEX_CONST_PIN_LOOP(_LIST) auto List = _LIST; for (const auto Constant : List) { PCGEX_PIN_PARAM(Constant.Name, "...", Normal, {}) }

	// Boolean pins
	if (ConstantList == EPCGExConstantListID::Booleans) { PCGEX_CONST_PIN_LOOP(PCGExConstants::Booleans) }
	// Vector pins (just axes for now)
	else if (ConstantList == EPCGExConstantListID::Vectors) { PCGEX_CONST_PIN_LOOP(GetVectorConstantList(ConstantList).Constants) }
	// Numerics
	else { PCGEX_CONST_PIN_LOOP(GetNumericConstantList(ConstantList).Constants) }

#undef PCGEX_CONST_PIN_LOOP

	return PinProperties;
}

FPCGElementPtr UPCGExConstantsSettings::CreateElement() const
{
	return MakeShared<FPCGExConstantsElement>();
}

bool FPCGExConstantsElement::ExecuteInternal(FPCGContext* InContext) const
{
	PCGEX_CONTEXT()
	PCGEX_SETTINGS(Constants)

#define PCGEX_OUTPUT_PARAM_DATA(_TYPE, _BODY)\
	UPCGParamData* OutputData = Context->ManagedObjects->New<UPCGParamData>();\
	check(OutputData && OutputData->Metadata);\
	_TYPE Value = static_cast<_TYPE>(Constant.Value); _BODY\
	FPCGMetadataAttribute<_TYPE>* Attrib = OutputData->Metadata->CreateAttribute<_TYPE>(Constant.Name, Value, true, false);\
	Attrib->SetValue(OutputData->Metadata->AddEntry(), Value);\
	Context->StageOutput(Constant.Name, OutputData, true);

	// Boolean constant outputs
	if (Settings->ConstantList == EPCGExConstantListID::Booleans)
	{
		for (const auto Constant : PCGExConstants::Booleans) { PCGEX_OUTPUT_PARAM_DATA(bool, {}) }
	}
	// Vector constant output
	else if (Settings->ConstantList == EPCGExConstantListID::Vectors)
	{
		auto ConstantsList = UPCGExConstantsSettings::GetVectorConstantList(Settings->ConstantList);
		for (const auto Constant : ConstantsList.Constants) { PCGEX_OUTPUT_PARAM_DATA(FVector, {if (Settings->NegateOutput) { Value = Value * -1.0; }}) }
	}
	// Numeric constant output
	else
	{
#define PCGEX_POST_PROCESS_NUMERIC(_ID, _TYPE)\
		if(Settings->NumericOutputType == EPCGExNumericOutput::_ID){ PCGEX_OUTPUT_PARAM_DATA(_TYPE, { PostProcessNumericValue(Value, Settings->NegateOutput, Settings->OutputReciprocal, Settings->CustomMultiplier); }) }

		auto ConstantsList = UPCGExConstantsSettings::GetNumericConstantList(Settings->ConstantList);
		for (const auto Constant : ConstantsList.Constants) { PCGEX_FOREACH_NUMERIC_OUTPUT(PCGEX_POST_PROCESS_NUMERIC) }

#undef PCGEX_POST_PROCESS_NUMERIC
	}

	Context->Done();
	return Context->TryComplete();
}

FPCGContext* FPCGExConstantsElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExContext* Context = new FPCGExContext();

	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;

	return Context;
}
