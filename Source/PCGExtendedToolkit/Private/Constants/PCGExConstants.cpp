// Copyright 2024 Edward Boucher and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Constants/PCGExConstants.h"
#include "PCGContext.h"
#include "PCGParamData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"

#if WITH_EDITOR
FName UPCGExConstantsSettings::GetDefaultNodeName() const {
	return FName("Constant");
}

FLinearColor UPCGExConstantsSettings::GetNodeTitleColor() const {
	return FLinearColor(0.2, 0.2, 0.2, 1.0);
}

FText UPCGExConstantsSettings::GetDefaultNodeTitle() const {
	return FText::FromString("PGCEx | Constant");
}

TArray<FPCGPreConfiguredSettingsInfo> UPCGExConstantsSettings::GetPreconfiguredInfo() const {
	return PCGMetadataElementCommon::FillPreconfiguredSettingsInfoFromEnum<EConstantListID>();
}
#endif

FString UPCGExConstantsSettings::GetAdditionalTitleInformation() const {
	if (const UEnum* EnumPtr = StaticEnum<EConstantListID>()) {
		return EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(ConstantList)).ToString();
	};
	return FString();
}

void UPCGExConstantsSettings::ApplyPreconfiguredSettings(const FPCGPreConfiguredSettingsInfo& PreconfigureInfo) {
	if (const UEnum* EnumPtr = StaticEnum<EConstantListID>()) {
		if (EnumPtr->IsValidEnumValue(PreconfigureInfo.PreconfiguredIndex)) {
			ConstantList = static_cast<EConstantListID>(PreconfigureInfo.PreconfiguredIndex);
		}
	}
}

TArray<FPCGPinProperties> UPCGExConstantsSettings::OutputPinProperties() const {
	
	TArray<FPCGPinProperties> Out;

	// Boolean pins
	if (ConstantList == EConstantListID::Booleans) {
		for (const auto Constant : PCGExConstants::Booleans) {
			auto Pin = Out.Emplace_GetRef(Constant.Name, EPCGDataType::Param, true, false);
		}
		return Out;
	}

	// Vector pins (just axes for now)
	if (ConstantList == EConstantListID::Vectors) {
		auto ConstantsList = GetVectorConstantList(ConstantList);
		for (const auto Constant : ConstantsList.Constants) {
			auto Pin = Out.Emplace_GetRef(Constant.Name, EPCGDataType::Param, true, false);
		}
		return Out;
	}
	
	// Numerics
	auto ConstantsList = GetNumericConstantList(ConstantList);
	for (const auto Constant : ConstantsList.Constants) {
		auto Pin = Out.Emplace_GetRef(Constant.Name, EPCGDataType::Param, true, false);
	}
	

	return Out;
}

FPCGElementPtr UPCGExConstantsSettings::CreateElement() const {
	return MakeShared<FPCGConstantsElement>();
}

FConstantDescriptorList<double> UPCGExConstantsSettings::GetNumericConstantList(EConstantListID ConstantList) {
	return PCGExConstants::Numbers.ExportedConstants[static_cast<uint8>(ConstantList)];
}

FConstantDescriptorList<FVector> UPCGExConstantsSettings::GetVectorConstantList(EConstantListID ConstantList) {
	return PCGExConstants::Vectors.ExportedConstants[0];
}

bool FPCGConstantsElement::ExecuteInternal(FPCGContext* Context) const {

	check (Context);

	const UPCGExConstantsSettings* Settings = Context->GetInputSettings<UPCGExConstantsSettings>();
	check(Settings);

	// Boolean constant outputs
	if (Settings->ConstantList == EConstantListID::Booleans) {
		for (const auto Constant : PCGExConstants::Booleans) {
			UPCGParamData* OutputData = FPCGContext::NewObject_AnyThread<UPCGParamData>(Context);
			check(OutputData && OutputData->Metadata);

			auto Attrib = OutputData->Metadata->CreateAttribute<bool>(Constant.Name, Constant.Value, true, false);
			OutputData->Metadata->AddEntry();
			auto Value = Constant.Value;
			Attrib->AddValue(Value);
		
			FPCGTaggedData& NewData = Context->OutputData.TaggedData.Emplace_GetRef();
			NewData.Data = OutputData;
			NewData.Pin = Constant.Name;
		}
	}
	
	// Vector constant output
	else if (Settings->ConstantList == EConstantListID::Vectors) {
		auto ConstantsList = UPCGExConstantsSettings::GetVectorConstantList(Settings->ConstantList);
		for (const auto Constant : ConstantsList.Constants) {
			UPCGParamData* OutputData = FPCGContext::NewObject_AnyThread<UPCGParamData>(Context);
			check(OutputData && OutputData->Metadata);

			auto Attrib = OutputData->Metadata->CreateAttribute<FVector>(Constant.Name, Constant.Value, true, false);
			OutputData->Metadata->AddEntry();

			auto Value = Constant.Value;

			if (Settings->NegateOutput) {
				Value = Value * -1.0;
			}

			Attrib->AddValue(Value * Settings->CustomMultiplier);
		
			FPCGTaggedData& NewData = Context->OutputData.TaggedData.Emplace_GetRef();
			NewData.Data = OutputData;
			NewData.Pin = Constant.Name;
		}
	}
	
	// Numeric constant output
	else {
		auto ConstantsList = UPCGExConstantsSettings::GetNumericConstantList(Settings->ConstantList);
		for (const auto Constant : ConstantsList.Constants) {
			UPCGParamData* OutputData = FPCGContext::NewObject_AnyThread<UPCGParamData>(Context);
			check(OutputData && OutputData->Metadata);
			
			auto Value = Constant.Value;

			if (Settings->NegateOutput) {
				Value = Value * -1.0;
			}
			
			if (Settings->OutputReciprocal) {
				if (!FMath::IsNearlyZero(Value)) {
					Value = 1.0 / Value;
				}
				else {
					Value = 0.0;
				}
				
			}
			
			Value = Value * Settings->CustomMultiplier;

			FPCGMetadataAttribute<double>* DoubleAttrib = nullptr;
			FPCGMetadataAttribute<float>* FloatAttrib = nullptr;
			FPCGMetadataAttribute<int>* Int32Attrib = nullptr;
			FPCGMetadataAttribute<int64>* Int64Attrib = nullptr;

			switch (Settings->NumericOutputType) {
				case ENumericOutput::Double:
					DoubleAttrib = OutputData->Metadata->CreateAttribute<double>(Constant.Name, Value, true, false);
					DoubleAttrib->AddValue(Value);
					break;
			
				case ENumericOutput::Float:
					FloatAttrib = OutputData->Metadata->CreateAttribute<float>(Constant.Name, Value, true, false);
					FloatAttrib->AddValue(Value);
					break;

				case ENumericOutput::Int32:
					Int32Attrib = OutputData->Metadata->CreateAttribute<int32>(Constant.Name, FMath::RoundToInt32(Value), true, false);
					Int32Attrib->AddValue(FMath::RoundToInt32(Value));
					break;

				case ENumericOutput::Int64:
					Int64Attrib = OutputData->Metadata->CreateAttribute<int64>(Constant.Name, FMath::RoundToInt64(Value), true, false);
					Int64Attrib->AddValue(FMath::RoundToInt64(Value));
					break;
			}
			
			OutputData->Metadata->AddEntry();
			FPCGTaggedData& NewData = Context->OutputData.TaggedData.Emplace_GetRef();
			NewData.Data = OutputData;
			NewData.Pin = Constant.Name;
		}
	}
	
	return true;
}
