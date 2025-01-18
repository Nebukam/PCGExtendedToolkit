// Copyright 2025 Edward Boucher and contributors
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

	// Boolean pins
	if (ConstantList == EPCGExConstantListID::Booleans)
	{
		for (auto List = PCGExConstants::Booleans;
		     const auto Constant : List)
		{
			PCGEX_PIN_PARAM(Constant.Name, "...", Normal, {})
		}
	}
	// Vector pins (just axes for now)
	else if (ConstantList == EPCGExConstantListID::Vectors)
	{
		for (auto List = GetVectorConstantList(ConstantList).Constants;
		     const auto Constant : List)
		{
			PCGEX_PIN_PARAM(Constant.Name, "...", Normal, {})
		}
	}
	// Numerics
	else
	{
		for (auto List = GetNumericConstantList(ConstantList).Constants;
		     const auto Constant : List)
		{
			PCGEX_PIN_PARAM(Constant.Name, "...", Normal, {})
		}
	}

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

	// Boolean constant outputs
	if (Settings->ConstantList == EPCGExConstantListID::Booleans)
	{
		for (const auto Constant : PCGExConstants::Booleans)
		{
			StageConstant(Context, Constant.Name, Constant.Value);
		}
	}
	// Vector constant output
	else if (Settings->ConstantList == EPCGExConstantListID::Vectors)
	{
		for (auto ConstantsList = UPCGExConstantsSettings::GetVectorConstantList(Settings->ConstantList);
		     const auto Constant : ConstantsList.Constants)
		{
			StageConstant(Context, Constant.Name, Settings->NegateOutput ? Constant.Value * -1 : Constant.Value);
		}
	}
	// Numeric constant output
	else
	{
		auto ConstantsList = UPCGExConstantsSettings::GetNumericConstantList(Settings->ConstantList);
		for (const auto Constant : ConstantsList.Constants)
		{
			auto Value = Settings->ApplyNumericValueSettings(Constant.Value);

			switch (Settings->NumericOutputType)
			{
			case EPCGExNumericOutput::Double: StageConstant<double>(Context, Constant.Name, Value);
				break;
			case EPCGExNumericOutput::Float: StageConstant<float>(Context, Constant.Name, Value);
				break;
			case EPCGExNumericOutput::Int32: StageConstant<int32>(Context, Constant.Name, Value);
				break;
			case EPCGExNumericOutput::Int64: StageConstant<int64>(Context, Constant.Name, Value);
				break;
			}
		}
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
