// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExHelpers.h"
#include "PCGExPointIO.h"
#include "PCGExDetails.h"

UENUM(BlueprintType)
enum class EPCGExNumericOutput : uint8
{
	Double = 0,
	Float  = 1,
	Int32  = 2,
	Int64  = 3,
};

namespace PCGExDataHelpers
{
	template <typename T>
	static T ReadDataValue(const FPCGMetadataAttribute<T>* Attribute)
	{
		const FPCGMetadataAttribute<T>* Attr = Attribute;
		if (!Attr->GetNumberOfEntries())
		{
			const FPCGMetadataAttribute<T>* Parent = Attr->GetParent();
			while (Parent)
			{
				if (!Parent->GetNumberOfEntries()) { Parent = Parent->GetParent(); }
				else
				{
					Attr = Parent;
					Parent = nullptr;
				}
			}
		}
		return !Attr->GetNumberOfEntries() ? Attr->GetValue(PCGDefaultValueKey) : Attr->GetValueFromItemKey(PCGFirstEntryKey);
	}

	template <typename T>
	static T ReadDataValue(const FPCGMetadataAttributeBase* Attribute, T Fallback)
	{
		T Value = Fallback;
		PCGEx::ExecuteWithRightType(
			Attribute->GetTypeId(), [&](auto DummyValue)
			{
				using T_VALUE = decltype(DummyValue);
				Value = PCGEx::Convert<T>(ReadDataValue(static_cast<const FPCGMetadataAttribute<T>*>(Attribute)));
			});
		return Value;
	}

	template <typename T>
	static void SetDataValue(FPCGMetadataAttribute<T>* Attribute, const T Value)
	{
		Attribute->SetValue(PCGFirstEntryKey, Value);
		Attribute->SetDefaultValue(Value);
	}

	constexpr static EPCGMetadataTypes GetNumericType(const EPCGExNumericOutput InType)
	{
		switch (InType)
		{
		case EPCGExNumericOutput::Double:
			return EPCGMetadataTypes::Double;
		case EPCGExNumericOutput::Float:
			return EPCGMetadataTypes::Float;
		case EPCGExNumericOutput::Int32:
			return EPCGMetadataTypes::Integer32;
		case EPCGExNumericOutput::Int64:
			return EPCGMetadataTypes::Integer64;
		}

		return EPCGMetadataTypes::Unknown;
	}

	template <typename T>
	static bool TryReadDataValue(FPCGExContext* InContext, const UPCGData* InData, const FPCGAttributePropertyInputSelector& InSelector, T& OutValue, bool bQuiet = false)
	{
		bool bSuccess = false;
		const UPCGMetadata* InMetadata = InData->Metadata;

		if (!InMetadata) { return false; }

		PCGEx::FSubSelection SubSelection(InSelector);
		FPCGAttributeIdentifier SanitizedIdentifier = PCGEx::GetAttributeIdentifier<true>(InSelector, InData);

		if (const FPCGMetadataAttributeBase* SourceAttribute = InMetadata->GetConstAttribute(SanitizedIdentifier))
		{
			PCGEx::ExecuteWithRightType(
				SourceAttribute->GetTypeId(), [&](auto DummyValue)
				{
					using T_VALUE = decltype(DummyValue);

					const FPCGMetadataAttribute<T_VALUE>* TypedSource = static_cast<const FPCGMetadataAttribute<T_VALUE>*>(SourceAttribute);
					const T_VALUE Value = ReadDataValue(TypedSource);

					if (SubSelection.bIsValid) { OutValue = SubSelection.Get<T_VALUE, T>(Value); }
					else { OutValue = PCGEx::Convert<T_VALUE, T>(Value); }

					bSuccess = true;
				});
		}
		else
		{
			if (InContext && !bQuiet) { PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid attribute: \"{0}\"."), FText::FromString(PCGEx::GetSelectorDisplayName(InSelector)))); }
			return false;
		}

		return bSuccess;
	}

	template <typename T>
	static bool TryReadDataValue(FPCGExContext* InContext, const UPCGData* InData, const FName& InName, T& OutValue, bool bQuiet = false)
	{
		FPCGAttributePropertyInputSelector Selector;
		Selector.Update(InName.ToString());
		return TryReadDataValue<T>(InContext, InData, Selector.CopyAndFixLast(InData), OutValue, bQuiet);
	}

	template <typename T>
	static bool TryReadDataValue(const TSharedPtr<PCGExData::FPointIO>& InIO, const FName& InName, T& OutValue, bool bQuiet = false)
	{
		PCGEX_SHARED_CONTEXT(InIO->GetContextHandle())
		return TryReadDataValue(SharedContext.Get(), InIO->GetIn(), InName, OutValue, bQuiet);
	}

	template <typename T>
	static bool TryReadDataValue(const TSharedPtr<PCGExData::FPointIO>& InIO, const FPCGAttributePropertyInputSelector& InSelector, T& OutValue, bool bQuiet = false)
	{
		PCGEX_SHARED_CONTEXT(InIO->GetContextHandle())
		return TryReadDataValue(SharedContext.Get(), InIO->GetIn(), InSelector, OutValue, bQuiet);
	}

	template <typename T>
	static bool TryGetSettingDataValue(
		FPCGExContext* InContext, const UPCGData* InData, const EPCGExInputValueType Input,
		const FPCGAttributePropertyInputSelector& InSelector, const T& InConstant, T& OutValue)
	{
		if (Input == EPCGExInputValueType::Constant)
		{
			OutValue = InConstant;
			return true;
		}

		return TryReadDataValue<T>(InContext, InData, InSelector, OutValue);
	}

	template <typename T>
	static bool TryGetSettingDataValue(
		FPCGExContext* InContext, const UPCGData* InData, const EPCGExInputValueType Input,
		const FName& InName, const T& InConstant, T& OutValue)
	{
		if (Input == EPCGExInputValueType::Constant)
		{
			OutValue = InConstant;
			return true;
		}

		FPCGAttributePropertyInputSelector Selector;
		Selector.Update(InName.ToString());
		return TryReadDataValue<T>(InContext, InData, Selector.CopyAndFixLast(InData), OutValue);
	}

	template <typename T>
	static bool TryGetSettingDataValue(
		const TSharedPtr<PCGExData::FPointIO>& InIO, const EPCGExInputValueType Input,
		const FName& InName, const T& InConstant, T& OutValue)
	{
		PCGEX_SHARED_CONTEXT(InIO->GetContextHandle())
		return TryGetSettingDataValue(SharedContext.Get(), InIO->GetIn(), Input, InName, InConstant, OutValue);
	}

	template <typename T>
	static bool TryGetSettingDataValue(
		const TSharedPtr<PCGExData::FPointIO>& InIO, const EPCGExInputValueType Input,
		const FPCGAttributePropertyInputSelector& InSelector, const T& InConstant, T& OutValue)
	{
		PCGEX_SHARED_CONTEXT(InIO->GetContextHandle())
		return TryGetSettingDataValue(SharedContext.Get(), InIO->GetIn(), Input, InSelector, InConstant, OutValue);
	}
}
